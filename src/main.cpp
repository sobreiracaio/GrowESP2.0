#include "Libraries.hpp"
#include "WifiManager.hpp"
#include "Time.hpp"
#include "FBase.hpp"
#include "Display.hpp"
#include "Button.hpp"
#include "DataClass.hpp"
#include "OTA.hpp"
#include "Light.hpp"
#include "Serial.hpp"




// ── Forward declarations ──────────────────────────────────────────────────────
static bool _firebaseReady = false;
bool hasInternet();
void firebase_healthy_startup();



// ── Objetos globais ───────────────────────────────────────────────────────────
Preferences prefs;
UpdateClass update;

TFT_eSPI    *tft     = NULL;
Display     *display = NULL;
FBase       *firebase = NULL;
Button      *button[4] = {NULL};

Light       light;
WifiManager wifi(&prefs, nullptr);
Time        rtc(&wifi, "pool.ntp.org", "pool.ntp.br", -10800, 0);
DataClass   data_class(&light, &prefs);
OTA         ota(&prefs, firebase, display);

struct tm now       = {0};
String    safeEmail = "";
int       menu      = -1;

// ── Wake request — setado pela ISR dos botões ─────────────────────────────────
volatile bool _wakeRequest = false;

void IRAM_ATTR onAnyButton()
{
    // Debounce mínimo na ISR — ignora pulsos muito rápidos (ruído elétrico)
    static unsigned long lastISR = 0;
    unsigned long now = millis();
    if (now - lastISR < 50) return;
    lastISR = now;
    _wakeRequest = true;
}

// Macro: aborta sendAndReceiveData se usuário pressionou botão
#define CHECK_WAKE()      do { if (_wakeRequest) return; } while(0)
#define CHECK_WAKE_BOOL() do { if (_wakeRequest) return false; } while(0)

// Flag que indica setup concluído — segura o menu até tudo estar pronto
static bool _setupDone = false;

// ── Receive ──────────────────────────────────────────────────────────────────
static bool _rcvActive = false;

// ── Tabela de paths — única definição, sem duplicata ─────────────────────────
struct PathEntry {
    const char* path;
    bool needsPrefix;
};

static const PathEntry _paths[] = {
    {"/_Binary",                                               false},
    {"/_Token",                                                false},
    {"/_HasUpdate",                                            false},
    {"/InsertedData/Light/HourOn",                             true},
    {"/InsertedData/Light/HourOff",                            true},
    {"/InsertedData/Sensor/Temperature/TargetTemp",            true},
    {"/InsertedData/Sensor/Temperature/TempTolerance",         true},
    {"/InsertedData/Sensor/Humid/TargetHumid",                 true},
    {"/InsertedData/Sensor/Humid/HumidTolerance",              true},
    {"/InsertedData/Sensor/Soil/TargetSoil",                   true},
    {"/InsertedData/Sensor/Soil/PumpDuration",                 true},
    {"/InsertedData/Sensor/Soil/AbsorptionDelay",              true},
    {"/InsertedData/Sensor/Soil/SoilTolerance",                true},
    {"/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry",   true},
    {"/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet",   true},
    {"/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow",    true},
    // Behavior removido — rega sempre por timer
    {"/InsertedData/Sensor/WaterReserv/Calibration/Reserv",    true},
    {"/InsertedData/Sensor/WaterReserv/Calibration/Capacity",  true},
    {"/Version",                                               true},
    {"/Status",                                                true},
};
static const int _pathsCount = sizeof(_paths) / sizeof(_paths[0]);

// ── Buffer de path reutilizável — sem alocação dinâmica nos loops ─────────────
static char _pathBuf[128] = {};

// Monta path em _pathBuf e retorna ponteiro para ele
static const char* buildPath(int i)
{
    if (_paths[i].needsPrefix)
        snprintf(_pathBuf, sizeof(_pathBuf), "%s%s", safeEmail.c_str(), _paths[i].path);
    else
        snprintf(_pathBuf, sizeof(_pathBuf), "%s", _paths[i].path);
    return _pathBuf;
}

// ── applyReceivedData ─────────────────────────────────────────────────────────
// Recebe char* em vez de String& para evitar cópia extra
static void applyReceivedData(int i, const char* data)
{
    switch (i) {
        case 0:  ota.setBinaryPath(data); break;
        case 1:  ota.setToken(data); break;
        case 2:  ota.setHasUpdate(strcmp(data, "true") == 0); break;
        case 3: {
            String s(data);
            int c = s.indexOf(':');
            if (c != -1) {
                data_class.setDayTime(s.substring(0,c).toInt(), s.substring(c+1).toInt());
                data_class.getDayTime(display->day);
            }
            break;
        }
        case 4: {
            String s(data);
            int c = s.indexOf(':');
            if (c != -1) {
                data_class.setNightTime(s.substring(0,c).toInt(), s.substring(c+1).toInt());
                data_class.getNightTime(display->night);
            }
            break;
        }
        case 5:  data_class.setTargetTemp(atof(data)); break;
        case 6:  data_class.setTempTolerance(atof(data)); break;
        case 7:  data_class.setTargetHumid(atof(data)); break;
        case 8:  data_class.setHumidTolerance(atof(data)); break;
        case 9:  data_class.setTargetSoil(atof(data)); break;
        case 10: data_class.setPumpDuration(atof(data)); break;
        case 11: data_class.setAbsorptionDelay(atof(data)); break;
        case 12: data_class.setSoilTolerance(atof(data)); break;
        case 13: data_class.setSoilLow(atoi(data)); break;
        case 14: data_class.setSoilUpper(atoi(data)); break;
        case 15: data_class.setPumpFlow(atof(data)); break;
        // case 16: Behavior removido — rega sempre por timer
        case 17: data_class.setWaterRawReading(atof(data)); break;
        case 18: data_class.setWaterCapacity(atof(data)); break;
        case 19: ota.setVersion(data); break;
        case 20: data_class.setIsRunning(strcmp(data, "true") == 0); break;
    }
}

// ── initClasses / initModules ─────────────────────────────────────────────────
void initClasses()
{
    tft       = new TFT_eSPI();
    button[0] = new Button(BTN1);
    button[1] = new Button(BTN2);
    button[2] = new Button(BTN3);
    button[3] = new Button(BTN4);
    display   = new Display(tft, firebase, &rtc, &ota, &wifi, button, &data_class);
    wifi.injectDisplay(display);
}

void initModules()
{
    display->initDisplay();
    firebase = new FBase(API_KEY, DATABASE_URL, wifi.getEmail(), wifi.getPass());
    display->injectFBase(firebase);
    ota.injectFbase(firebase);
    ota.injectDisplay(display);
    for (int i = 0; i < 4; i++)
        button[i]->init();

    // ISR para acordar tela imediatamente — FALLING pois pull-ups externos
    attachInterrupt(digitalPinToInterrupt(BTN1), onAnyButton, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN2), onAnyButton, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN3), onAnyButton, FALLING);
    attachInterrupt(digitalPinToInterrupt(BTN4), onAnyButton, FALLING);
}

// ── getNow ────────────────────────────────────────────────────────────────────
// Uma única chamada atômica de getLocalTime() garante que todos os campos
// de 'now' pertencem ao mesmo instante — evita inconsistências quando o
// RTC avança entre chamadas separadas (ex: tm_sec=59, tm_min do minuto seguinte).
void getNow()
{
    now = rtc.getTimeStruct(); // getLocalTime() atômico — um único snapshot
    rtc.checkSync();
}

// ── sendDataStartUP ───────────────────────────────────────────────────────────
void sendDataStartUP()
{
    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    uint8_t arrID[10]  = {TT, TTOL, TH, HTOL, TS, STOL, PD, AD, LIGHT0, STATUS0};
    float   arrVal[10] = {
        data_class.getTargetTemp(),
        data_class.getTempTolerance(),
        data_class.getTargetHumid(),
        data_class.getHumidTolerance(),
        data_class.getTargetSoil(),
        data_class.getSoilTolerance(),
        data_class.getPumpDuration(),
        data_class.getAbsorptionDelay() / 60.0f,  // segundos → minutos (uint16_t max=65535s)
        lightValue,
        (float)data_class.getIsRunning()
    };

    // Envia pacotes com throttle não-bloqueante — sendPacket já tem 50ms interno
    // mas como são IDs diferentes o throttle não bloqueia, apenas garante espaçamento
    for (int i = 0; i < 10; i++) {
        esp_task_wdt_reset();
        unsigned long t = millis();
        while (millis() - t < 60) { esp_task_wdt_reset(); yield(); }
        sendPacket(arrID[i], arrVal[i]);
    }
    sendPacket(PUMP_CAL, false);
}

// ── receiveFirebaseDataFast ───────────────────────────────────────────────────
// Faz 1 GET no nó raiz do usuário + 3 GETs nos nós globais.
// Total: 4 requisições SSL ao invés de 21 — startup ~4× mais rápido.
bool receiveFirebaseDataFast()
{
    esp_task_wdt_reset();
    CHECK_WAKE_BOOL();

    // ── 1. GET nó raiz do usuário (contém InsertedData, Version, Status) ──
    String userJson;
    if (!firebase->dbGetRaw(safeEmail, userJson)) {
        Serial.println("[RCV Fast] Falhou GET raiz usuario, usando receive lento.");
        return false;
    }

    esp_task_wdt_reset();

    // Parse do JSON do usuário com ArduinoJson
    // O nó raiz pode ter ~2KB — DynamicJsonDocument com 4KB é suficiente
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, userJson);
    if (err) {
        Serial.printf("[RCV Fast] JSON parse erro: %s\n", err.c_str());
        return false;
    }

    // Helpers para extrair valores com fallback seguro
    auto getStr = [&](JsonVariant v) -> String {
        if (v.isNull()) return "";
        return v.as<String>();
    };
    auto getFloat = [&](JsonVariant v, float def = 0.0f) -> float {
        if (v.isNull()) return def;
        return v.as<float>();
    };

    // InsertedData/Light
    JsonObject light_obj = doc["InsertedData"]["Light"];
    {
        String hon = getStr(light_obj["HourOn"]);
        if (hon.length() > 0) {
            int c = hon.indexOf(':');
            if (c != -1) {
                data_class.setDayTime(hon.substring(0,c).toInt(), hon.substring(c+1).toInt());
                data_class.getDayTime(display->day);
            }
        }
        String hoff = getStr(light_obj["HourOff"]);
        if (hoff.length() > 0) {
            int c = hoff.indexOf(':');
            if (c != -1) {
                data_class.setNightTime(hoff.substring(0,c).toInt(), hoff.substring(c+1).toInt());
                data_class.getNightTime(display->night);
            }
        }
    }

    // InsertedData/Sensor/Temperature
    JsonObject temp_obj = doc["InsertedData"]["Sensor"]["Temperature"];
    data_class.setTargetTemp(getFloat(temp_obj["TargetTemp"]));
    data_class.setTempTolerance(getFloat(temp_obj["TempTolerance"]));

    // InsertedData/Sensor/Humid
    JsonObject humid_obj = doc["InsertedData"]["Sensor"]["Humid"];
    data_class.setTargetHumid(getFloat(humid_obj["TargetHumid"]));
    data_class.setHumidTolerance(getFloat(humid_obj["HumidTolerance"]));

    // InsertedData/Sensor/Soil
    JsonObject soil_obj = doc["InsertedData"]["Sensor"]["Soil"];
    data_class.setTargetSoil(getFloat(soil_obj["TargetSoil"]));
    data_class.setPumpDuration(getFloat(soil_obj["PumpDuration"]));
    data_class.setAbsorptionDelay(getFloat(soil_obj["AbsorptionDelay"]));
    data_class.setSoilTolerance(getFloat(soil_obj["SoilTolerance"]));

    // InsertedData/Sensor/Soil/Calibration
    JsonObject cal_obj = soil_obj["Calibration"];
    data_class.setSoilLow(cal_obj["SoilSensor"]["Dry"].as<int>());
    data_class.setSoilUpper(cal_obj["SoilSensor"]["Wet"].as<int>());
    data_class.setPumpFlow(getFloat(cal_obj["Pump"]["PumpFlow"]));
    data_class.setSoilBehavior((int)getFloat(cal_obj["Behavior"]));

    // InsertedData/Sensor/WaterReserv
    JsonObject water_obj = doc["InsertedData"]["Sensor"]["WaterReserv"]["Calibration"];
    data_class.setWaterRawReading(getFloat(water_obj["Reserv"]));
    data_class.setWaterCapacity(getFloat(water_obj["Capacity"]));

    // Version e Status
    ota.setVersion(getStr(doc["Version"]).c_str());
    data_class.setIsRunning(doc["Status"].as<bool>());

    doc.clear();
    esp_task_wdt_reset();

    // ── 2. GETs nos nós globais (/_Binary, /_Token, /_HasUpdate) ──────
    String val;
    CHECK_WAKE_BOOL();
    if (firebase->dbGet("/_Binary",    val)) ota.setBinaryPath(val.c_str());
    esp_task_wdt_reset();
    CHECK_WAKE_BOOL();
    if (firebase->dbGet("/_Token",     val)) ota.setToken(val.c_str());
    esp_task_wdt_reset();
    CHECK_WAKE_BOOL();
    if (firebase->dbGet("/_HasUpdate", val)) ota.setHasUpdate(val == "true");
    esp_task_wdt_reset();

    return true;
}

// ── receiveFirebaseData ───────────────────────────────────────────────────────
// Usa buffer fixo — zero alocação dinâmica de String no loop
void receiveFirebaseData()
{
    char dataBuf[256] = {};

    for (int i = 0; i < _pathsCount; i++) {
        esp_task_wdt_reset();

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[RCV] WiFi perdido, abortando receive.");
            return;
        }

        dataBuf[0] = '\0';
        firebase->awaitGet(buildPath(i), dataBuf, (unsigned int)sizeof(dataBuf));
        applyReceivedData(i, dataBuf);
    }
}

// ── sendActuatorsIfChanged ───────────────────────────────────────────────────
// Chamado a cada loop — envia imediatamente ao Firebase se algum atuador mudou.
// Independente do throttle de 30s dos sensores.
void sendActuatorsIfChanged()
{
    if (!wifi.getStatus()) return;
    if (!firebase->isReady()) return;
    if (ESP.getMaxAllocHeap() < 20000) return;

    static const char* apaths[6] = {
        "/Readings/Actuator/LightStatus",
        "/Readings/Actuator/PumpStatus",
        "/Readings/Actuator/CoolerStatus",
        "/Readings/Actuator/HeaterStatus",
        "/Readings/Actuator/HumidStatus",
        "/Readings/Actuator/DehumidStatus"
    };

    bool acts[6] = {
        data_class.getLightStatus(),   data_class.getPumpStatus(),
        data_class.getCoolerStatus(),  data_class.getHeaterStatus(),
        data_class.getHumidStatus(),   data_class.getDehumidStatus()
    };
    static bool last_acts[6] = {false, false, false, false, false, false};

    char fullPath[128];
    for (int j = 0; j < 6; j++) {
        if (acts[j] != last_acts[j]) {
            CHECK_WAKE();
            snprintf(fullPath, sizeof(fullPath), "%s%s", safeEmail.c_str(), apaths[j]);
            firebase->dbSet(fullPath, acts[j]);
            last_acts[j] = acts[j];
            esp_task_wdt_reset();
        }
    }
}

// ── sendAndReceiveData ────────────────────────────────────────────────────────
void sendAndReceiveData()
{
    if (!wifi.getStatus()) return;
    if (!firebase->isReady()) return;
    if (ESP.getMaxAllocHeap() < 20000) return;

    esp_task_wdt_reset();

    // ── FASE RECEIVE — ativado por HasChange == true ─────────────────
    if (_rcvActive) {
        if (WiFi.status() != WL_CONNECTED) { _rcvActive = false; return; }
        // Receive silencioso — sem logoScreen para não acender tela durante idle
        if (!receiveFirebaseDataFast()) {
            Serial.println("[HasChange] Fast receive falhou, usando receive lento...");
            receiveFirebaseData();
        }
        esp_task_wdt_reset();
        firebase->dbSet(safeEmail + "/HasChange", String("false"));
        data_class.setHasChange("false");
        data_class.saveToPrefs();
        _rcvActive = false;
        // Só redesenha se tela estiver acesa
        if (ledcRead(0) > 0) {
            display->drawBackGround();
            display->invalidateButtonScreen();
        }
        return;
    }

    // ── FASE SEND sensores (throttle 30s) ─────────────────────────────
    static unsigned long lastSend = 0;
    if (millis() - lastSend >= 60000) {
        lastSend = millis();

        static const char* spaths[5] = {
            "/Readings/Sensor/Temperature",
            "/Readings/Sensor/Humidity",
            "/Readings/Sensor/Soil",
            "/Readings/Sensor/WaterReserv",
            "/Readings/Sensor/VPD"
        };

        float sens[5] = {
            data_class.getTemp(), data_class.getHumid(),
            data_class.getCalibratedSoil(), data_class.getWaterCalibrated(),
            data_class.getVPD()
        };

        char fullPath[128];
        for (int i = 0; i < 5; i++) {
            CHECK_WAKE();
            snprintf(fullPath, sizeof(fullPath), "%s%s", safeEmail.c_str(), spaths[i]);
            firebase->dbSet(fullPath, sens[i]);
            esp_task_wdt_reset();
        }
    }

    // ── VERIFICAR HasChange (throttle 15s) ────────────────────────────
    static unsigned long lastHasChangeCheck = 0;
    if (millis() - lastHasChangeCheck < 60000) return;
    lastHasChangeCheck = millis();

    CHECK_WAKE();
    char hcBuf[16] = {};
    char hcPath[128];
    snprintf(hcPath, sizeof(hcPath), "%s/HasChange", safeEmail.c_str());
    firebase->awaitGet(hcPath, hcBuf, (unsigned int)sizeof(hcBuf));
    data_class.setHasChange(String(hcBuf));

    if (strcmp(hcBuf, "true") == 0) {
        _rcvActive = true;
    }
}

// ── ButtonIdle ────────────────────────────────────────────────────────────────
void ButtonIdle()
{
    static int brightness = 255;
    static unsigned long lastStep = 0;
    const unsigned long stepInterval = 1;

    esp_task_wdt_reset();

    // ISR setou flag — inicia fade-in gradual e reseta idle de todos os botões
    static bool waking = false;
    if (_wakeRequest) {
        _wakeRequest = false;
        waking = true;
        for (int i = 0; i < 4; i++)
            button[i]->idleButton();
        if (menu == -2) menu = -1;
    }

    bool idle = !waking &&
                button[0]->getIdle() && button[1]->getIdle() &&
                button[2]->getIdle() && button[3]->getIdle();

    if (waking) {
        // Fade-in — mesmo ritmo do fade-out
        if (brightness < 255 && millis() - lastStep > stepInterval) {
            lastStep = millis();
            brightness++;
            ledcWrite(0, brightness);
        }
        if (brightness >= 255)
            waking = false;
    } else if (idle) {
        esp_task_wdt_reset();
        if (brightness > 0 && millis() - lastStep > stepInterval) {
            lastStep = millis();
            brightness--;
            ledcWrite(0, brightness);
        }

        for (int i = 0; i < 4; i++) {
            if (button[i]->read()) break;
        }

        if (brightness == 0) {
            menu = -2;

            // Renovação de token só com tela apagada
            if (_setupDone && firebase && firebase->isReady()) firebase->loop();

            // Retentativa de Firebase em background — init silencioso sem logoScreen
            if (_setupDone && !_firebaseReady && wifi.getStatus()) {
                static unsigned long lastRetry = 0;
                if (lastRetry == 0) lastRetry = millis();
                if (millis() - lastRetry > 60000UL) {
                    lastRetry = millis();
                    Serial.println("[Firebase] Tentando conectar em background...");
                    if (hasInternet()) {
                        bool ok = firebase->init() && firebase->isReady();
                        if (ok) {
                            _firebaseReady = true;
                            Serial.println("[Firebase] Conectado em background!");
                        }
                    }
                }
            }

            display->healthCheck();
            sendAndReceiveData();
            esp_task_wdt_reset();
            wifi.loop();
        }
    } else {
        if (brightness < 255 && millis() - lastStep > stepInterval) {
            lastStep = millis();
            brightness++;
            ledcWrite(0, brightness);
        }
    }
}

// ── initFirebaseStructure ─────────────────────────────────────────────────────
void initFirebaseStructure()
{
    if (!wifi.getStatus()) return;

    char statusBuf[16] = {};
    char statusPath[128];
    snprintf(statusPath, sizeof(statusPath), "%s/Status", safeEmail.c_str());

    String statusPathStr(statusPath);
    String statusResult;
    firebase->awaitGet(statusPathStr, &statusResult);

    String verStr = ota.getVersion();
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/Version", verStr);

    if (statusResult == "true" || statusResult == "false") {
        display->logoScreen("Dados existentes carregados!");
        delay(500);
        return;
    }

    display->logoScreen("Primeiro acesso, criando estrutura...");
    esp_task_wdt_reset();

    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/Status", false);
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/email",   wifi.getEmail());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/Version", verStr);
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Light/HourOn",  String("08:00"));
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Light/HourOff", String("22:00"));
    esp_task_wdt_reset();

    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp",    data_class.getTargetTemp());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", data_class.getTempTolerance());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid",         data_class.getTargetHumid());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance",      data_class.getHumidTolerance());
    esp_task_wdt_reset();

    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil",       data_class.getTargetSoil());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance",    data_class.getSoilTolerance());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration",     data_class.getPumpDuration());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay",  data_class.getAbsorptionDelay());
    esp_task_wdt_reset();

    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry",  (float)data_class.getSoilLow());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet",  (float)data_class.getSoilUpper());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow",   data_class.getPumpFlow());
    esp_task_wdt_reset();
    // Behavior removido — rega sempre por timer
    esp_task_wdt_reset();

    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Reserv",   data_class.getWaterRawReading());
    esp_task_wdt_reset();
    firebase->dbSet(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Capacity", data_class.getWaterCapacity());

    display->logoScreen("Estrutura criada com sucesso!");
    delay(1000);
}

// ── wdt_conf ──────────────────────────────────────────────────────────────────
// ── firebaseTask ─────────────────────────────────────────────────────────────
// Task dedicada ao Firebase. Roda no core 0, com watchdog próprio de 60s.
// Isola qualquer bloqueio SSL do loopTask — se travar, só ela é reiniciada
void wdt_conf()
{
    esp_task_wdt_deinit();
    esp_task_wdt_init(60, false);
    esp_task_wdt_add(NULL);
}

// ── Checagem de internet real (além do WiFi) ──────────────────────────────────
// Tenta TCP connect na porta 53 do 8.8.8.8 (DNS Google).
// Não usa ICMP — funciona no ESP32 sem permissões especiais.
// Timeout de 3s para não bloquear muito o boot.
bool hasInternet()
{
    if (WiFi.status() != WL_CONNECTED) return false;
    WiFiClient client;
    client.setTimeout(3000);
    bool ok = client.connect(IPAddress(8,8,8,8), 53);
    if (ok) client.stop();
    return ok;
}

// ── Flag de Firebase inicializado ─────────────────────────────────────────────
// (declarada como forward declaration no topo do arquivo)

// ── firebase_healthy_startup ──────────────────────────────────────────────────
void firebase_healthy_startup()
{
    // Tenta conectar ao Firebase com backoff: 2s → 4s → 8s (3 tentativas)
    const int MAX_INIT_ATTEMPTS = 3;
    unsigned long backoff = 2000;
    bool initOk = false;

    for (int i = 1; i <= MAX_INIT_ATTEMPTS; i++) {
        esp_task_wdt_reset();
        display->logoScreen("Conectando Firebase... (" + String(i) + "/" + String(MAX_INIT_ATTEMPTS) + ")");
        initOk = firebase->init() && firebase->isReady() && firebase->isHealthy();
        if (initOk) break;

        // Credenciais inválidas — não adianta tentar de novo, abre portal imediatamente
        if (firebase->hasCredentialError()) {
            display->logoScreen("Email ou senha invalidos! Reconfigure...");
            Serial.println("[Firebase] Credenciais invalidas. Abrindo portal de configuracao.");
            delay(2000);
            // Apaga credenciais salvas para forçar reconfiguração
            prefs.begin("wifi", false);
            prefs.remove("email");
            prefs.remove("userpass");
            prefs.end();
            wifi.startPortal(); // bloqueia aqui até o usuário salvar (ou timeout de 5min + restart)
            return;
        }

        Serial.printf("[Firebase] init() falhou (tentativa %d). Aguardando %lus...\n", i, backoff / 1000);
        unsigned long w = millis();
        while (millis() - w < backoff) { esp_task_wdt_reset(); yield(); }
        backoff *= 2;
    }

    if (!initOk) {
        display->logoScreen("Firebase indisponivel. Continuando offline...");
        delay(2000);
        return; // Sobe sem Firebase — firebaseReconnectLoop() cuida do resto
    }

    const int MAX_ATTEMPTS = 3;
    bool structureOk = false;
    bool receiveOk   = false;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        esp_task_wdt_reset();

        if (!structureOk) {
            display->logoScreen("Carregando estrutura... (" + String(attempt) + "/" + String(MAX_ATTEMPTS) + ")");
            initFirebaseStructure();
            esp_task_wdt_reset();
            structureOk = true;
        }

        if (!receiveOk) {
            display->logoScreen("Recebendo dados... (" + String(attempt) + "/" + String(MAX_ATTEMPTS) + ")");
            bool fastOk = receiveFirebaseDataFast();
            if (!fastOk) {
                Serial.println("[Startup] Fast receive falhou, usando receive lento...");
                receiveFirebaseData();
            }
            Serial.printf("[Startup] Receive %s | TargetHumid=%.1f TargetTemp=%.1f\n",
                fastOk ? "fast" : "slow",
                data_class.getTargetHumid(), data_class.getTargetTemp());
            esp_task_wdt_reset();
            receiveOk = true;
        }

        if (structureOk && receiveOk) break;
    }

    if (!structureOk || !receiveOk) {
        display->logoScreen("Falha na inicializacao, continuando...");
        delay(2000);
    }

    data_class.saveToPrefs();
    Serial.println("[Boot] Dados salvos nas Preferences apos receive.");
    display->logoScreen("Conectado ao banco de dados!");
    _firebaseReady = true;
}

// ── heapMonitor ───────────────────────────────────────────────────────────────
void heapMonitor()
{
    static unsigned long lastHeapLog = 0;
    if (millis() - lastHeapLog < 30000) return;
    lastHeapLog = millis();

    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxAllocHeap();

    // ── Stack do loopTask ─────────────────────────────────────────────
    // uxTaskGetStackHighWaterMark(NULL) retorna o mínimo de words livres
    // que já houve no stack desta task desde que foi criada.
    // Cada word = 4 bytes no ESP32. Abaixo de 512 words (2KB) é crítico.
    UBaseType_t stackWatermark = uxTaskGetStackHighWaterMark(NULL);

    Serial.printf("[HEAP] Free: %lu | Min: %lu | MaxBlock: %lu | StackMin: %lu words (%lu bytes)\n",
        (unsigned long)freeHeap,
        (unsigned long)ESP.getMinFreeHeap(),
        (unsigned long)maxBlock,
        (unsigned long)stackWatermark,
        (unsigned long)stackWatermark * 4);

    // Stack crítico — stack overflow iminente
    if (stackWatermark < 512) {
        Serial.printf("[STACK] CRITICO! Minimo historico: %lu words. Reiniciando preventivamente.\n",
                      (unsigned long)stackWatermark);
        Serial.flush();
        ESP.restart();
    }

    // Fragmentação crítica: livre mas sem bloco contíguo
    if (freeHeap > 20000 && maxBlock < 10000) {
        Serial.println("[HEAP] Fragmentacao critica, reiniciando...");
        Serial.flush();
        ESP.restart();
    }

    if (freeHeap < 15000) {
        Serial.println("[HEAP] Heap critico, reiniciando...");
        Serial.flush();
        ESP.restart();
    }
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup()
{
    wdt_conf();
    initClasses();
    initModules();

    display->logoScreen("Inicializando sistemas de armazenamento...");
    delay(1000);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    Serial.begin(115200);

    // ── Log da causa do último reset ──────────────────────────────────
    {
        esp_reset_reason_t reason = esp_reset_reason();
        const char* reasonStr = "DESCONHECIDO";
        switch (reason) {
            case ESP_RST_POWERON:   reasonStr = "POWER_ON (alimentacao)";       break;
            case ESP_RST_EXT:       reasonStr = "RESET_EXTERNO (pino EN)";      break;
            case ESP_RST_SW:        reasonStr = "RESET_SOFTWARE (ESP.restart)"; break;
            case ESP_RST_PANIC:     reasonStr = "PANIC (crash/excecao)";        break;
            case ESP_RST_INT_WDT:   reasonStr = "WATCHDOG_INTERRUPCAO";         break;
            case ESP_RST_TASK_WDT:  reasonStr = "WATCHDOG_TAREFA";              break;
            case ESP_RST_WDT:       reasonStr = "WATCHDOG_OUTRO";               break;
            case ESP_RST_DEEPSLEEP: reasonStr = "DEEP_SLEEP";                  break;
            case ESP_RST_BROWNOUT:  reasonStr = "BROWNOUT (queda de tensao!)";  break;
            case ESP_RST_SDIO:      reasonStr = "SDIO";                         break;
            default:                                                             break;
        }
        Serial.printf("\n[RESET] Causa do ultimo reset: %s (codigo %d)\n", reasonStr, (int)reason);

        // Exibe no display por 2s para ser visível mesmo sem Serial conectado
        display->logoScreen(String("Reset: ") + reasonStr);
        delay(2000);
    }

    display->logoScreen("Ajustando protocolos de comunicacao...");
    delay(500);

    Serial2.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
    Serial2.flush();

    display->logoScreen("Inicializando equipamento...");
    delay(500);
    light.setTimeFunction(getNow);

    wifi.ensureCredentialsLoaded();

    if (wifi.checkCredentials()) {
        display->logoScreen("Tentando conectar a: " + wifi.getSSID());
        if (wifi.wifiInit()) {
            rtc.begin();
            display->logoScreen("Conectado a internet!");
            delay(500);
            display->logoScreen("Conectando ao banco de dados! Aguarde!");
        }

        safeEmail = wifi.getEmail();
        safeEmail.replace(".", "_");

        // Verifica se há internet real antes de tentar Firebase
        if (hasInternet()) {
            // Firebase tem prioridade — Preferences só como fallback
            firebase_healthy_startup();
            if (!_firebaseReady) {
                // Firebase falhou — carrega Preferences como fallback
                data_class.loadFromPrefs();
                data_class.getDayTime(display->day);
                data_class.getNightTime(display->night);
                Serial.println("[Boot] Firebase falhou, usando Preferences como fallback.");
            }
        } else {
            // Sem internet — carrega Preferences imediatamente
            data_class.loadFromPrefs();
            data_class.getDayTime(display->day);
            data_class.getNightTime(display->night);
            Serial.println("[Boot] Sem internet. Usando Preferences como fallback.");
            display->logoScreen("Sem internet. Dados locais carregados!");
            delay(2000);
        }
    } else {
        display->logoScreen("Internet nao configurada, indo para configuracao");
        menu = 6;
    }

    if (!wifi.getStatus() && !rtc.getStatus())
        display->logoScreen("Iniciando! Internet nao conectada, sem relogio!");
    else
        display->logoScreen("Iniciando! Aguarde!");

    sendDataStartUP();

    display->logoScreen("Versao: " + ota.getVersion());
    ota.fetchReleaseInfo();
    display->resetPulseTimer();
    _setupDone = true;
}



// ── loop ──────────────────────────────────────────────────────────────────────
void loop()
{
    esp_task_wdt_reset();
    heapMonitor();

    if (wifi.getStatus() && rtc.getStatus())
        getNow();

    sendActuatorsIfChanged();
    ButtonIdle();

    // Leitura serial — janela de 50ms para não bloquear
    {
        static unsigned long lastPacketTime = 0;
        unsigned long serialStart = millis();
        bool gotPacket = false;

        while (Serial2.available() >= 5 && millis() - serialStart < 50) {
            readPacket(&data_class);
            gotPacket = true;
        }

        if (gotPacket) {
            lastPacketTime = millis();
        } else if (lastPacketTime > 0 && millis() - lastPacketTime > 120000) {
            // Sem pacote por 2 minutos — sensor pode estar travado
            // Invalida leituras para não enviar dados velhos ao Firebase
            Serial.println("[SENSOR] Sem pacotes ha 2min. Invalidando leituras.");
            data_class.setTemp(-1);
            data_class.setHumid(-1);
            lastPacketTime = millis(); // evita spam de log
        }
    }

    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    sendPacket(LIGHT0, lightValue);
    light.run(data_class.getIsRunning());

    static int last_menu = -2;
    if (menu < -1) menu = -1;

    if (last_menu != menu) {
        display->drawBackGround();
        display->invalidateButtonScreen();
        last_menu = menu;
    }

    switch (menu) {
        case -1: display->mainScreen(&menu);        break;
        case  0: display->monitorMenu(&menu);       break;
        case  1: display->lightMenu(&menu);         break;
        case  2: display->tempMenu(&menu);          break;
        case  3: display->humidMenu(&menu);         break;
        case  4:
            display->soilMenuTimer(&menu);
            break;
        case  5: display->calibrationScreen(&menu); break;
        case  6: display->setupScreen(&menu);       break;
        default: break;
    }

}