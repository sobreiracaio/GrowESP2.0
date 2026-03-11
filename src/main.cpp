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

// ── Receive incremental ───────────────────────────────────────────────────────
static int  _rcvIndex  = -1;
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
    {"/InsertedData/Sensor/Soil/Calibration/Behavior",         true},
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
        case 16: data_class.setSoilBehavior((int)atof(data)); break;
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
}

// ── getNow ────────────────────────────────────────────────────────────────────
void getNow()
{
    now.tm_hour = rtc.getHour();
    now.tm_min  = rtc.getMinute();
    now.tm_sec  = rtc.getSecond();
    now.tm_mday = rtc.getDay();
    now.tm_mon  = rtc.getMonth();
    now.tm_year = rtc.getYear();
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
        data_class.getAbsorptionDelay(),
        lightValue,
        (float)data_class.getIsRunning()
    };

    for (int i = 0; i < 10; i++) {
        esp_task_wdt_reset();
        sendPacket(arrID[i], arrVal[i]);
        delay(500);
    }
    sendPacket(PUMP_CAL, false);
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
        firebase->awaitGet(buildPath(i), dataBuf, sizeof(dataBuf));
        applyReceivedData(i, dataBuf);
    }
}

// ── sendAndReceiveData ────────────────────────────────────────────────────────
void sendAndReceiveData()
{
    if (!wifi.getStatus()) return;
    if (!firebase || !firebase->isReady()) return;
    if (ESP.getMaxAllocHeap() < 20000) return;

    esp_task_wdt_reset();

    // ── FASE RECEIVE incremental (1 path por chamada) ─────────────────
    if (_rcvActive) {
        if (WiFi.status() != WL_CONNECTED) {
            _rcvActive = false;
            _rcvIndex  = -1;
            return;
        }

        if (_rcvIndex < _pathsCount) {
            char dataBuf[256] = {};
            firebase->awaitGet(buildPath(_rcvIndex), dataBuf, sizeof(dataBuf));
            esp_task_wdt_reset();
            applyReceivedData(_rcvIndex, dataBuf);
            esp_task_wdt_reset();
            _rcvIndex++;
        } else {
            // Concluiu todos os paths
            firebase->aSyncSetString(safeEmail + "/HasChange", String("false"));
            data_class.setHasChange("false");
            _rcvActive = false;
            _rcvIndex  = -1;
        }
        return;
    }

    // ── FASE SEND (throttle 30s) ──────────────────────────────────────
    static unsigned long lastSend = 0;
    if (millis() - lastSend >= 30000) {
        lastSend = millis();

        if (data_class.getIsRunning()) {
            static float last_sensors[4] = {-1,-1,-1,-1};
            static bool  last_act[6]     = {false,false,false,false,false,false};

            // Paths como literais — sem alocação
            static const char* spaths[4] = {
                "/Readings/Sensor/Temperature",
                "/Readings/Sensor/Humidity",
                "/Readings/Sensor/Soil",
                "/Readings/Sensor/WaterReserv"
            };
            static const char* apaths[6] = {
                "/Readings/Actuator/LightStatus",
                "/Readings/Actuator/PumpStatus",
                "/Readings/Actuator/CoolerStatus",
                "/Readings/Actuator/HeaterStatus",
                "/Readings/Actuator/HumidStatus",
                "/Readings/Actuator/DehumidStatus"
            };

            float sens[4] = {
                data_class.getTemp(), data_class.getHumid(),
                data_class.getCalibratedSoil(), data_class.getWaterCalibrated()
            };
            bool acts[6] = {
                data_class.getLightStatus(), data_class.getPumpStatus(),
                data_class.getCoolerStatus(), data_class.getHeaterStatus(),
                data_class.getHumidStatus(), data_class.getDehumidStatus()
            };

            // Monta path em buffer fixo — sem String temporária no loop
            char fullPath[128];
            for (int i = 0; i < 4; i++) {
                if (sens[i] != last_sensors[i]) {
                    snprintf(fullPath, sizeof(fullPath), "%s%s", safeEmail.c_str(), spaths[i]);
                    firebase->aSyncSetFloat(fullPath, sens[i]);
                    last_sensors[i] = sens[i];
                    esp_task_wdt_reset();
                }
            }
            for (int j = 0; j < 6; j++) {
                if (acts[j] != last_act[j]) {
                    snprintf(fullPath, sizeof(fullPath), "%s%s", safeEmail.c_str(), apaths[j]);
                    firebase->aSyncSetBool(fullPath, acts[j]);
                    last_act[j] = acts[j];
                    esp_task_wdt_reset();
                }
            }
        }
    }

    // ── VERIFICAR HasChange (throttle 15s) ────────────────────────────
    static unsigned long lastHasChangeCheck = 0;
    if (millis() - lastHasChangeCheck < 15000) return;
    lastHasChangeCheck = millis();

    char hcBuf[16] = {};
    char hcPath[128];
    snprintf(hcPath, sizeof(hcPath), "%s/HasChange", safeEmail.c_str());
    firebase->awaitGet(hcPath, hcBuf, sizeof(hcBuf));
    data_class.setHasChange(String(hcBuf));

    if (strcmp(hcBuf, "true") == 0) {
        _rcvActive = true;
        _rcvIndex  = 0;
    }
}

// ── ButtonIdle ────────────────────────────────────────────────────────────────
void ButtonIdle()
{
    static int brightness = 255;
    static unsigned long lastStep = 0;
    const unsigned long stepInterval = 1;

    esp_task_wdt_reset();

    bool idle = button[0]->getIdle() && button[1]->getIdle() &&
                button[2]->getIdle() && button[3]->getIdle();

    if (idle) {
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
    firebase->aSyncSetString(safeEmail + "/Version", verStr);

    if (statusResult == "true" || statusResult == "false") {
        display->logoScreen("Dados existentes carregados!");
        delay(500);
        return;
    }

    display->logoScreen("Primeiro acesso, criando estrutura...");
    esp_task_wdt_reset();

    firebase->aSyncSetBool  (safeEmail + "/Status", false);
    firebase->aSyncSetString(safeEmail + "/email",   wifi.getEmail());
    firebase->aSyncSetString(safeEmail + "/Version", verStr);
    firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOn",  String("08:00"));
    firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOff", String("22:00"));
    esp_task_wdt_reset();

    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp",    data_class.getTargetTemp());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", data_class.getTempTolerance());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid",         data_class.getTargetHumid());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance",      data_class.getHumidTolerance());
    esp_task_wdt_reset();

    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil",       data_class.getTargetSoil());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance",    data_class.getSoilTolerance());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration",     data_class.getPumpDuration());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay",  data_class.getAbsorptionDelay());
    esp_task_wdt_reset();

    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry",  (float)data_class.getSoilLow());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet",  (float)data_class.getSoilUpper());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow",   data_class.getPumpFlow());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Behavior",        (float)data_class.getSoilBehavior());
    esp_task_wdt_reset();

    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Reserv",   data_class.getWaterRawReading());
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Capacity", data_class.getWaterCapacity());

    display->logoScreen("Estrutura criada com sucesso!");
    delay(1000);
}

// ── wdt_conf ──────────────────────────────────────────────────────────────────
void wdt_conf()
{
    esp_task_wdt_deinit();
    esp_task_wdt_init(30, false);
    esp_task_wdt_add(NULL);
}

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
    const unsigned long OP_TIMEOUT = 20000;
    bool structureOk = false;
    bool receiveOk   = false;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        esp_task_wdt_reset();

        if (!structureOk) {
            unsigned long t = millis();
            display->logoScreen("Carregando estrutura... (" + String(attempt) + "/" + String(MAX_ATTEMPTS) + ")");
            initFirebaseStructure();
            esp_task_wdt_reset();
            structureOk = (millis() - t < OP_TIMEOUT);
            if (!structureOk) continue;
        }

        if (!receiveOk) {
            unsigned long t = millis();
            display->logoScreen("Recebendo dados... (" + String(attempt) + "/" + String(MAX_ATTEMPTS) + ")");
            receiveFirebaseData();
            esp_task_wdt_reset();
            receiveOk = (millis() - t < OP_TIMEOUT);
            if (!receiveOk) continue;
        }

        if (structureOk && receiveOk) break;
    }

    if (!structureOk || !receiveOk) {
        display->logoScreen("Falha na inicializacao, continuando...");
        delay(2000);
        // Reinicia apenas se o problema é no Firebase, não na rede
        if (WiFi.status() == WL_CONNECTED)
            ESP.restart();
    }

    display->logoScreen("Conectado ao banco de dados!");
}

// ── heapMonitor ───────────────────────────────────────────────────────────────
void heapMonitor()
{
    static unsigned long lastHeapLog = 0;
    if (millis() - lastHeapLog < 30000) return;
    lastHeapLog = millis();

    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxAllocHeap();

    Serial.printf("[HEAP] Free: %lu | Min: %lu | MaxBlock: %lu\n",
        (unsigned long)freeHeap,
        (unsigned long)ESP.getMinFreeHeap(),
        (unsigned long)maxBlock);

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

        firebase_healthy_startup();
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
    delay(1000);
    ota.fetchReleaseInfo();
}

// ── firebaseReconnectLoop ─────────────────────────────────────────────────────
// Ponto único de reconexão Firebase. Backoff exponencial: 2s → 4s → 8s → ... → 64s
// Evita martelo no servidor quando Firebase está fora (não o WiFi).
void firebaseReconnectLoop()
{
    if (!wifi.getStatus() || !firebase) return;
    if (firebase->isBusy()) return;

    static unsigned long _lastAttempt    = 0;
    static unsigned long _backoff        = 2000;  // começa em 2s
    static bool          _waitingToInit  = false;
    static unsigned long _wifiStableSince = 0;    // quando o WiFi ficou estável

    // ── Rastreia estabilidade do WiFi ─────────────────────────────────
    // Reseta o contador sempre que o WiFi cai para garantir que só
    // tentamos init() depois de 3s contínuos de conexão estável.
    static bool _lastWifiStatus = false;
    bool wifiNow = wifi.getStatus();
    if (!wifiNow) {
        _lastWifiStatus  = false;
        _wifiStableSince = 0;
        return;
    }
    if (!_lastWifiStatus && wifiNow) {
        // WiFi acabou de (re)conectar — marca o instante
        _wifiStableSince = millis();
        _lastWifiStatus  = true;
        Serial.println("[Firebase] WiFi reconectado. Aguardando estabilizacao (3s)...");
    }
    // Ainda dentro da janela de estabilização — não toca no Firebase
    if (millis() - _wifiStableSince < 3000) return;

    // ── Heap baixo: força reconexão imediata ──────────────────────────
    bool heapLow = ESP.getFreeHeap() < 30000;
    if (heapLow && firebase->isHealthy()) {
        Serial.printf("[HEAP] Baixo (%lu), forcando reconexao Firebase...\n",
                      (unsigned long)ESP.getFreeHeap());
        esp_task_wdt_reset();
        firebase->stopApp();
        esp_task_wdt_reset();
        _waitingToInit = true;
        _lastAttempt   = millis();
        _backoff       = 2000;
        return;
    }

    // ── Firebase saudável: reseta backoff ─────────────────────────────
    if (firebase->isHealthy()) {
        _backoff       = 2000;
        _waitingToInit = false;
        return;
    }

    // ── Firebase não saudável: inicia espera se ainda não iniciou ─────
    if (!_waitingToInit) {
        esp_task_wdt_reset();
        firebase->stopApp();
        esp_task_wdt_reset();
        _waitingToInit = true;
        _lastAttempt   = millis();
        Serial.printf("[Firebase] Reconectando em %lus...\n", _backoff / 1000);
        return;
    }

    // ── Aguarda backoff antes de tentar init ──────────────────────────
    if (millis() - _lastAttempt < _backoff) return;

    Serial.println("[Firebase] Tentando reconectar...");
    esp_task_wdt_reset();
    bool ok = firebase->init();
    esp_task_wdt_reset();

    if (ok) {
        Serial.println("[Firebase] Reconectado com sucesso!");
        _backoff       = 2000;
        _waitingToInit = false;
    } else {
        // Backoff exponencial com teto de 64s
        _backoff = min(_backoff * 2, (unsigned long)64000);
        _lastAttempt = millis();
        Serial.printf("[Firebase] Falha. Proxima tentativa em %lus.\n", _backoff / 1000);
    }
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop()
{
    esp_task_wdt_reset();
    heapMonitor();

    if (wifi.getStatus() && rtc.getStatus())
        getNow();

    firebaseReconnectLoop();

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
        last_menu = menu;
    }

    switch (menu) {
        case -1: display->mainScreen(&menu);        break;
        case  0: display->monitorMenu(&menu);       break;
        case  1: display->lightMenu(&menu);         break;
        case  2: display->tempMenu(&menu);          break;
        case  3: display->humidMenu(&menu);         break;
        case  4:
            if (data_class.getSoilBehavior() == SENSOR) display->soilMenuSensor(&menu);
            if (data_class.getSoilBehavior() == TIMER)  display->soilMenuTimer(&menu);
            break;
        case  5: display->calibrationScreen(&menu); break;
        case  6: display->setupScreen(&menu);       break;
        default: break;
    }
}