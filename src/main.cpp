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
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ── Fila de comandos para a firebaseTask ─────────────────────────────────────
// O loopTask enfileira comandos (set/get) e a firebaseTask os executa.
// Isso isola completamente o SSL/Firebase do loop principal.
enum FbCmdType { FB_SET_STRING, FB_SET_FLOAT, FB_SET_BOOL, FB_GET_STRING };

struct FbCmd {
    FbCmdType   type;
    char        path[128];
    char        sval[64];   // string value ou buffer para GET
    float       fval;
    bool        bval;
};

static QueueHandle_t  _fbQueue    = NULL;   // loopTask → firebaseTask
static SemaphoreHandle_t _fbMutex = NULL;   // protege acesso ao objeto firebase
static volatile bool  _fbTaskAlive = false; // firebaseTask está rodando

// ── Forward declarations ──────────────────────────────────────────────────────
static void fbSet(const String& path, const String& val);
static void fbSet(const String& path, float val);
static void fbSet(const String& path, bool val);
static bool fbReady();
static bool fbTryLock();
static void fbUnlock();
static void fbAwaitGet(String path, String* result);
static void fbAwaitGet(const char* path, char* buf, unsigned int len);

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
        fbAwaitGet(buildPath(i), dataBuf, (unsigned int)sizeof(dataBuf));
        applyReceivedData(i, dataBuf);
    }
}

// ── sendAndReceiveData ────────────────────────────────────────────────────────
void sendAndReceiveData()
{
    if (!wifi.getStatus()) return;
    if (!fbReady()) return;
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
            fbAwaitGet(buildPath(_rcvIndex), dataBuf, (unsigned int)sizeof(dataBuf));
            esp_task_wdt_reset();
            applyReceivedData(_rcvIndex, dataBuf);
            esp_task_wdt_reset();
            _rcvIndex++;
        } else {
            // Concluiu todos os paths
            fbSet(safeEmail + "/HasChange", String("false"));
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
                    fbSet(fullPath, sens[i]);
                    last_sensors[i] = sens[i];
                    esp_task_wdt_reset();
                }
            }
            for (int j = 0; j < 6; j++) {
                if (acts[j] != last_act[j]) {
                    snprintf(fullPath, sizeof(fullPath), "%s%s", safeEmail.c_str(), apaths[j]);
                    fbSet(fullPath, acts[j]);
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
    fbAwaitGet(hcPath, hcBuf, (unsigned int)sizeof(hcBuf));
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
    fbAwaitGet(statusPathStr, &statusResult);

    String verStr = ota.getVersion();
    fbSet(safeEmail + "/Version", verStr);

    if (statusResult == "true" || statusResult == "false") {
        display->logoScreen("Dados existentes carregados!");
        delay(500);
        return;
    }

    display->logoScreen("Primeiro acesso, criando estrutura...");
    esp_task_wdt_reset();

    fbSet(safeEmail + "/Status", false);
    fbSet(safeEmail + "/email",   wifi.getEmail());
    fbSet(safeEmail + "/Version", verStr);
    fbSet(safeEmail + "/InsertedData/Light/HourOn",  String("08:00"));
    fbSet(safeEmail + "/InsertedData/Light/HourOff", String("22:00"));
    esp_task_wdt_reset();

    fbSet(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp",    data_class.getTargetTemp());
    fbSet(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", data_class.getTempTolerance());
    fbSet(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid",         data_class.getTargetHumid());
    fbSet(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance",      data_class.getHumidTolerance());
    esp_task_wdt_reset();

    fbSet(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil",       data_class.getTargetSoil());
    fbSet(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance",    data_class.getSoilTolerance());
    fbSet(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration",     data_class.getPumpDuration());
    fbSet(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay",  data_class.getAbsorptionDelay());
    esp_task_wdt_reset();

    fbSet(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry",  (float)data_class.getSoilLow());
    fbSet(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet",  (float)data_class.getSoilUpper());
    fbSet(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow",   data_class.getPumpFlow());
    fbSet(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Behavior",        (float)data_class.getSoilBehavior());
    esp_task_wdt_reset();

    fbSet(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Reserv",   data_class.getWaterRawReading());
    fbSet(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Capacity", data_class.getWaterCapacity());

    display->logoScreen("Estrutura criada com sucesso!");
    delay(1000);
}

// ── wdt_conf ──────────────────────────────────────────────────────────────────
// ── firebaseTask ─────────────────────────────────────────────────────────────
// Task dedicada ao Firebase. Roda no core 0, com watchdog próprio de 60s.
// Isola qualquer bloqueio SSL do loopTask — se travar, só ela é reiniciada
// via ESP.restart() após watchdog, sem corromper o stack do loop principal.
//
// Comunicação com o loopTask via fila _fbQueue:
//   loopTask  →  fbSet/fbGet (enfileira FbCmd)
//   firebaseTask  →  executa FBase::aSyncSet*/awaitGet
//
static void firebaseTask(void* pv)
{
    esp_task_wdt_add(NULL);
    Serial.println("[fbTask] Iniciada no core 0");
    _fbTaskAlive = true;

    // Estado da máquina de reconexão
    static unsigned long _lastAttempt  = 0;
    static unsigned long _backoff      = 2000;
    static unsigned long _wifiSince    = 0;
    static bool          _wifiWas      = false;
    static unsigned long _initDoneAt   = 0;  // quando o último init() OK terminou
    static bool          _needsInit    = false;

    FbCmd cmd;
    while (true) {
        esp_task_wdt_reset();

        bool wifiOk = (WiFi.status() == WL_CONNECTED);

        // ── Rastreia estabilidade do WiFi ────────────────────────────
        if (!wifiOk) {
            if (_wifiWas) {
                _wifiWas  = false;
                _needsInit = true;
                Serial.println("[fbTask] WiFi caiu. Marcando para reconexao.");
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (!_wifiWas) {
            _wifiSince = millis();
            _wifiWas   = true;
            Serial.println("[fbTask] WiFi conectado. Aguardando 3s...");
        }
        if (millis() - _wifiSince < 3000) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // ── Decide se precisa reconectar ─────────────────────────────
        // Usa isReady() (não isHealthy) — isHealthy exige ssl_client.connected()
        // que só fica true após a primeira operação de dados, causando loop falso.
        if (!firebase) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── Todos os acessos ao firebase protegidos por mutex ────────
        if (!xSemaphoreTake(_fbMutex, pdMS_TO_TICKS(200))) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        bool ready = firebase->isReady();
        if (ready && millis() - _initDoneAt > 5000) {
            if (!firebase->isHealthy()) {
                Serial.println("[fbTask] Firebase caiu. Agendando reconexao.");
                _needsInit = true;
                ready = false;
            }
        }
        xSemaphoreGive(_fbMutex);

        if (!ready || _needsInit) {
            if (millis() - _lastAttempt < _backoff) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            Serial.printf("[fbTask] Reconectando (backoff %lus)...\n", _backoff/1000);
            esp_task_wdt_reset();
            if (!xSemaphoreTake(_fbMutex, pdMS_TO_TICKS(5000))) {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            if (firebase->isReady()) firebase->stopApp();
            bool ok = firebase->init();
            xSemaphoreGive(_fbMutex);
            esp_task_wdt_reset();
            if (ok) {
                Serial.println("[fbTask] Firebase reconectado!");
                _backoff    = 2000;
                _needsInit  = false;
                _initDoneAt = millis();
                vTaskDelay(pdMS_TO_TICKS(2000));
            } else {
                _backoff = min(_backoff * 2, (unsigned long)64000);
                _lastAttempt = millis();
                Serial.printf("[fbTask] Falha. Proxima tentativa em %lus\n", _backoff/1000);
            }
            continue;
        }

        // ── Loop do Firebase ──────────────────────────────────────────
        if (!xSemaphoreTake(_fbMutex, pdMS_TO_TICKS(200))) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        esp_task_wdt_reset();
        firebase->loop();
        esp_task_wdt_reset();
        xSemaphoreGive(_fbMutex);

        // ── Processa fila de comandos ─────────────────────────────────
        if (_fbQueue && xQueueReceive(_fbQueue, &cmd, 0) == pdTRUE) {
            if (!xSemaphoreTake(_fbMutex, pdMS_TO_TICKS(200))) continue;
            if (firebase->isReady()) {
                esp_task_wdt_reset();
                switch (cmd.type) {
                    case FB_SET_STRING:
                        firebase->aSyncSetString(String(cmd.path), String(cmd.sval));
                        break;
                    case FB_SET_FLOAT:
                        firebase->aSyncSetFloat(String(cmd.path), cmd.fval);
                        break;
                    case FB_SET_BOOL:
                        firebase->aSyncSetBool(String(cmd.path), cmd.bval);
                        break;
                    default: break;
                }
                esp_task_wdt_reset();
            }
            xSemaphoreGive(_fbMutex);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// ── Helpers para enfileirar operações Firebase ────────────────────────────────
// O loopTask usa estas funções em vez de chamar firebase-> diretamente.
// São não-bloqueantes: apenas enfileiram o comando e retornam imediatamente.

static void fbSet(const String& path, const String& val)
{
    if (!_fbQueue) return;
    FbCmd cmd;
    cmd.type = FB_SET_STRING;
    strncpy(cmd.path, path.c_str(), sizeof(cmd.path) - 1);
    strncpy(cmd.sval, val.c_str(),  sizeof(cmd.sval) - 1);
    cmd.path[sizeof(cmd.path)-1] = 0;
    cmd.sval[sizeof(cmd.sval)-1] = 0;
    xQueueSend(_fbQueue, &cmd, 0);  // pdFALSE = descarta se fila cheia
}

static void fbSet(const String& path, float val)
{
    if (!_fbQueue) return;
    FbCmd cmd;
    cmd.type = FB_SET_FLOAT;
    strncpy(cmd.path, path.c_str(), sizeof(cmd.path) - 1);
    cmd.path[sizeof(cmd.path)-1] = 0;
    cmd.fval = val;
    xQueueSend(_fbQueue, &cmd, 0);
}

static void fbSet(const String& path, bool val)
{
    if (!_fbQueue) return;
    FbCmd cmd;
    cmd.type = FB_SET_BOOL;
    strncpy(cmd.path, path.c_str(), sizeof(cmd.path) - 1);
    cmd.path[sizeof(cmd.path)-1] = 0;
    cmd.bval = val;
    xQueueSend(_fbQueue, &cmd, 0);
}

// fbReady: o loopTask pode verificar se vale a pena enfileirar
// fbLock/fbUnlock — usados pelo loopTask para acessar firebase com segurança
// A firebaseTask usa o mesmo mutex antes de qualquer firebase->
// fbTryLock — tenta adquirir o mutex SEM bloquear (timeout zero).
// O loopTask NUNCA deve esperar pelo mutex — se a firebaseTask está ocupada,
// descarta a operação e tenta na próxima iteração. Isso garante que
// ButtonIdle, display e botões respondam sempre sem travamento.
static bool fbTryLock()
{
    if (!_fbMutex) return false;
    return xSemaphoreTake(_fbMutex, 0) == pdTRUE;
}
static void fbUnlock()
{
    if (_fbMutex) xSemaphoreGive(_fbMutex);
}

static bool fbReady()
{
    if (!firebase) return false;
    if (!fbTryLock()) return false;  // se ocupado, reporta "não pronto"
    bool r = firebase->isReady();
    fbUnlock();
    return r;
}

// fbAwaitGet — awaitGet não-bloqueante para o loopTask.
// Se o mutex não estiver disponível, retorna sem fazer nada.
static void fbAwaitGet(String path, String* result)
{
    if (!firebase || !fbTryLock()) return;
    firebase->awaitGet(path, result);
    fbUnlock();
}
static void fbAwaitGet(const char* path, char* buf, unsigned int len)
{
    if (!firebase || !fbTryLock()) return;
    firebase->awaitGet(path, buf, len);
    fbUnlock();
}

void wdt_conf()
{
    esp_task_wdt_deinit();
    esp_task_wdt_init(60, true);
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

        // Criar fila e mutex ANTES do startup para que fbAwaitGet
        // funcione corretamente durante receiveFirebaseData no boot.
        // A firebaseTask só é criada depois — durante o startup o mutex
        // fica livre e fbTryLock() retorna true normalmente.
        if (!_fbQueue) _fbQueue = xQueueCreate(20, sizeof(FbCmd));
        if (!_fbMutex) _fbMutex = xSemaphoreCreateMutex();

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

    // ── Lança a task dedicada do Firebase ────────────────────────────────────
    // A partir daqui o Firebase roda em task separada (core 0).
    // O loopTask (core 1) nunca mais chama firebase-> diretamente —
    // usa fbSet() para enfileirar e fbReady() para verificar estado.
    xTaskCreatePinnedToCore(
        firebaseTask,   // função da task
        "firebaseTask", // nome
        8192,           // stack 8KB — SSL + mbedTLS precisam de espaço
        NULL,
        2,              // prioridade 2 > loopTask (1) para não perder eventos
        NULL,
        0               // core 0 — loopTask fica no core 1
    );
    Serial.println("[Setup] firebaseTask criada no core 0");
}

// ── firebaseReconnectLoop ────────────────────────────────────────────────────
// Mantida por compatibilidade — reconexão agora gerenciada pela firebaseTask.
void firebaseReconnectLoop() { }

// ── loop ──────────────────────────────────────────────────────────────────────
void loop()
{
    esp_task_wdt_reset();
    heapMonitor();

    if (wifi.getStatus() && rtc.getStatus())
        getNow();

    // Firebase gerenciado pela firebaseTask — sem chamada aqui

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