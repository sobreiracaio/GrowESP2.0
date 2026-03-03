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




//Libraries
Preferences prefs;
UpdateClass update;

TFT_eSPI *tft = NULL;

//Classes, estruturas, variaveis e objetos
Light light;
WifiManager wifi(&prefs, nullptr);
Time rtc( &wifi, "pool.ntp.org", "pool.ntp.br", -10800, 0);
FBase *firebase = NULL;

Display *display = NULL;
Button* button[4] = {NULL};
DataClass data_class(&light, &prefs);
OTA ota(&prefs, firebase, display);

struct tm now = {0};
String safeEmail = "";
int menu = -1;

static int _rcvIndex   = -1;   // -1 = inativo
static bool _rcvActive = false;

struct PathEntry {
    const char* path;
    bool needsPrefix;
};

static const PathEntry _paths[] = {
    {"/_Binary",                                              false},
    {"/_Token",                                              false},
    {"/_HasUpdate",                                          false},
    {"/InsertedData/Light/HourOn",                           true},
    {"/InsertedData/Light/HourOff",                          true},
    {"/InsertedData/Sensor/Temperature/TargetTemp",          true},
    {"/InsertedData/Sensor/Temperature/TempTolerance",       true},
    {"/InsertedData/Sensor/Humid/TargetHumid",               true},
    {"/InsertedData/Sensor/Humid/HumidTolerance",            true},
    {"/InsertedData/Sensor/Soil/TargetSoil",                 true},
    {"/InsertedData/Sensor/Soil/PumpDuration",               true},
    {"/InsertedData/Sensor/Soil/AbsorptionDelay",            true},
    {"/InsertedData/Sensor/Soil/SoilTolerance",              true},
    {"/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry", true},
    {"/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet", true},
    {"/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow",  true},
    {"/InsertedData/Sensor/Soil/Calibration/Behavior",       true},
    {"/InsertedData/Sensor/WaterReserv/Calibration/Reserv",  true},
    {"/InsertedData/Sensor/WaterReserv/Calibration/Capacity",true},
    {"/Version",                                              true},
    {"/Status",                                              true},
};
static const int _pathsCount = sizeof(_paths) / sizeof(_paths[0]);





// ============ FUNÇÕES AUXILIARES ============
void initClasses() {
    tft = new TFT_eSPI();
    button[0] = new Button(BTN1);
    button[1] = new Button(BTN2);
    button[2] = new Button(BTN3);
    button[3] = new Button(BTN4);
    display = new Display(tft, firebase, &rtc, &ota, &wifi, button, &data_class); 
    wifi.injectDisplay(display);
}

void initModules() {
    display->initDisplay();
    firebase = new FBase(API_KEY, DATABASE_URL, wifi.getEmail(), wifi.getPass());
    display->injectFBase(firebase);
    ota.injectFbase(firebase);
    ota.injectDisplay(display);
    for (int i = 0; i < 4; i++)
        button[i]->init();
}

void getNow() {
    now.tm_hour = rtc.getHour();
    now.tm_min = rtc.getMinute();
    now.tm_sec = rtc.getSecond();
    now.tm_mday = rtc.getDay();
    now.tm_mon = rtc.getMonth();
    now.tm_year = rtc.getYear();
    rtc.checkSync();
}

void sendDataStartUP()
{
    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    uint8_t arrID[10] = {TT, TTOL, TH, HTOL, TS, STOL, PD, AD, LIGHT0, STATUS0};
    float arrVal[10] = {
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
    
    const int count = sizeof(arrID) / sizeof(arrID[0]);
    
    for (int i = 0; i < count; i++) {
        esp_task_wdt_reset();    
        sendPacket(arrID[i], arrVal[i]);
        delay(500);
        //feedWatchdog();
    }
    sendPacket(PUMP_CAL, false);
}

void formatDate(String *date, int period) {
    int colonIndex = date->indexOf(':');
    if (colonIndex == -1) return;
   
    float hour = date->substring(0, colonIndex).toFloat();
    float minute = date->substring(colonIndex + 1).toFloat();

    if(period == DAY) {
        data_class.setDayTime(hour, minute);
    }
    if(period == NIGHT) {
        data_class.setNightTime(hour, minute);
    }
}


void applyReceivedData(int i, const String& data)
{
    switch (i) {
        case 0:  ota.setBinaryPath(data); break;
        case 1:  ota.setToken(data); break;
        case 2:  ota.setHasUpdate(data == "true"); break;
        case 3:  formatDate(const_cast<String*>(&data), DAY);  data_class.getDayTime(display->day); break;
        case 4:  formatDate(const_cast<String*>(&data), NIGHT); data_class.getNightTime(display->night); break;
        case 5:  data_class.setTargetTemp(data.toFloat()); break;
        case 6:  data_class.setTempTolerance(data.toFloat()); break;
        case 7:  data_class.setTargetHumid(data.toFloat()); break;
        case 8:  data_class.setHumidTolerance(data.toFloat()); break;
        case 9:  data_class.setTargetSoil(data.toFloat()); break;
        case 10: data_class.setPumpDuration(data.toFloat()); break;
        case 11: data_class.setAbsorptionDelay(data.toFloat()); break;
        case 12: data_class.setSoilTolerance(data.toFloat()); break;
        case 13: data_class.setSoilLow(data.toInt()); break;
        case 14: data_class.setSoilUpper(data.toInt()); break;
        case 15: data_class.setPumpFlow(data.toFloat()); break;
        case 16: data_class.setSoilBehavior(data.toFloat()); break;
        case 17: data_class.setWaterRawReading(data.toFloat()); break;
        case 18: data_class.setWaterCapacity(data.toFloat()); break;
        case 19: ota.setVersion(data); break;
        case 20: data_class.setIsRunning(data == "true"); break;
    }
}

void sendAndReceiveData()
{
    if (!wifi.getStatus()) return;

    if (ESP.getFreeHeap() < 30000) 
    {
        Serial.printf("[HEAP] Baixo (%d), reconectando Firebase...\n", ESP.getFreeHeap());
        firebase->stopApp();
        delay(1000);
        firebase->init();
        return;
    }

    if (!firebase->isHealthy()) 
    {
        //Serial.println("[Firebase] Não saudável, reiniciando conexão...");
        firebase->stopApp();
        delay(500);
        firebase->init();
        return;
    }

    if (!firebase || !firebase->isReady()) return;
    if (ESP.getMaxAllocHeap() < 20000) {
        //Serial.printf("[HEAP] Critico (%d bytes) — abortando Firebase\n", ESP.getMaxAllocHeap());
        return;
    }

    esp_task_wdt_reset();

    // ── FASE RECEIVE (processa 1 path por chamada) ──────────────────
    if (_rcvActive) {
        if (_rcvIndex < _pathsCount) {
            char pathBuf[128];
            if (_paths[_rcvIndex].needsPrefix)
                snprintf(pathBuf, sizeof(pathBuf), "%s%s", safeEmail.c_str(), _paths[_rcvIndex].path);
            else
                snprintf(pathBuf, sizeof(pathBuf), "%s", _paths[_rcvIndex].path);

            String fullPath(pathBuf);
            String data;
            data.reserve(64);
            firebase->awaitGet(fullPath, &data);
            esp_task_wdt_reset();
            applyReceivedData(_rcvIndex, data);
            esp_task_wdt_reset();
            //Serial.printf("[RCV] %d/%d: %s = %s\n", _rcvIndex+1, _pathsCount, pathBuf, data.c_str());
            _rcvIndex++;
        } else {
            // Terminou todos os paths
            String temp = "false";
            firebase->aSyncSetString(safeEmail + "/HasChange", temp);
            data_class.setHasChange("false");
            _rcvActive = false;
            _rcvIndex  = -1;
            //Serial.println("[RCV] Receive completo.");
        }
        return; // ← sai sem fazer send neste ciclo
    }

    // ── FASE SEND (throttle 30s) ─────────────────────────────────────
    static unsigned long lastSend = 0;
    if (millis() - lastSend >= 30000) {
        lastSend = millis();

        if (data_class.getIsRunning()) {
            static float last_sensors[4] = {-1,-1,-1,-1};
            static bool  last_act[6]     = {false,false,false,false,false,false};

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

            float sens[4] = {data_class.getTemp(), data_class.getHumid(),
                             data_class.getCalibratedSoil(), data_class.getWaterCalibrated()};
            bool  acts[6] = {data_class.getLightStatus(), data_class.getPumpStatus(),
                             data_class.getCoolerStatus(), data_class.getHeaterStatus(),
                             data_class.getHumidStatus(), data_class.getDehumidStatus()};

            for (int i = 0; i < 4; i++) {
                if (sens[i] != last_sensors[i]) {
                    String p = safeEmail + spaths[i];
                    firebase->aSyncSetFloat(p, sens[i]);
                    last_sensors[i] = sens[i];
                    esp_task_wdt_reset();
                }
            }
            for (int j = 0; j < 6; j++) {
                if (acts[j] != last_act[j]) {
                    String p = safeEmail + apaths[j];
                    firebase->aSyncSetBool(p, acts[j]);
                    last_act[j] = acts[j];
                    esp_task_wdt_reset();
                }
            }
        }
    }

    // ── VERIFICAR HasChange ──────────────────────────────────────────
    static unsigned long lastHasChangeCheck = 0;
    if (millis() - lastHasChangeCheck < 15000) return;
    lastHasChangeCheck = millis();

    String hasChange;
    hasChange.reserve(8);
    firebase->awaitGet(safeEmail + "/HasChange", &hasChange);
    data_class.setHasChange(hasChange);

    if (hasChange == "true") {
        //Serial.println("[RCV] HasChange detectado, iniciando receive...");
        _rcvActive = true;
        _rcvIndex  = 0;
    }
}

void receiveFirebaseData()
{
    struct PathData {
        const char* path;
        bool needsPrefix;
    };
    
    PathData paths[] = {
        {"/_Binary", false},
        {"/_Token", false},
        {"/_HasUpdate", false},
        {"/InsertedData/Light/HourOn", true},
        {"/InsertedData/Light/HourOff", true},
        {"/InsertedData/Sensor/Temperature/TargetTemp", true},
        {"/InsertedData/Sensor/Temperature/TempTolerance", true},
        {"/InsertedData/Sensor/Humid/TargetHumid", true},
        {"/InsertedData/Sensor/Humid/HumidTolerance", true},
        {"/InsertedData/Sensor/Soil/TargetSoil", true},
        {"/InsertedData/Sensor/Soil/PumpDuration", true},
        {"/InsertedData/Sensor/Soil/AbsorptionDelay", true},  
        {"/InsertedData/Sensor/Soil/SoilTolerance", true},
        {"/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry", true},
        {"/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet", true},
        {"/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow", true},
        {"/InsertedData/Sensor/Soil/Calibration/Behavior", true},  
        {"/InsertedData/Sensor/WaterReserv/Calibration/Reserv", true},
        {"/InsertedData/Sensor/WaterReserv/Calibration/Capacity", true},
        {"/Version", true},
        {"/Status", true}
          
    };
    
    int paths_size = sizeof(paths) / sizeof(paths[0]);  // ✅ Tamanho correto

    for(int i = 0; i < paths_size; i++)
    {
        esp_task_wdt_reset();
        String fullPath = paths[i].needsPrefix ? 
                          safeEmail + paths[i].path : 
                          String(paths[i].path);
        
        String data = "";
        firebase->awaitGet(fullPath, &data);
        
        switch (i)
        {
            case 0:
                ota.setBinaryPath(data);
                break;
            
            case 1:
                ota.setToken(data);
                break;
            
            case 2:
                ota.setHasUpdate(data == "true");
                break;

            case 3:
                formatDate(&data, DAY);
                data_class.getDayTime(display->day);
                break;

            case 4:
                formatDate(&data, NIGHT);
                data_class.getNightTime(display->night);
                break;

            case 5:
                data_class.setTargetTemp(data.toFloat());
                break;

            case 6:
                data_class.setTempTolerance(data.toFloat());
                break;

            case 7:
                data_class.setTargetHumid(data.toFloat());
                break;

            case 8:
                data_class.setHumidTolerance(data.toFloat());
                break;

            case 9:
                data_class.setTargetSoil(data.toFloat());
                break;
            
            case 10:
                data_class.setPumpDuration(data.toFloat());
                break;
            
            case 11:
                data_class.setAbsorptionDelay(data.toFloat());
                break;
            
            case 12:
                data_class.setSoilTolerance(data.toFloat());
                break;
            
            case 13:
                data_class.setSoilLow(data.toInt());
                break;
            
            case 14:
                data_class.setSoilUpper(data.toInt());
                break;
            
            case 15:
                data_class.setPumpFlow(data.toFloat());
                break;
            
            case 16:
                data_class.setSoilBehavior(data.toFloat());
                break;
            
            case 17:
                data_class.setWaterRawReading(data.toFloat());
                break;

            case 18:
                data_class.setWaterCapacity(data.toFloat());
                break;
            
            case 19:
                ota.setVersion(data);
                break;
            
            case 20:
                data_class.setIsRunning(data == "true");
                //Serial.printf("Status : %d\n", data_class.getIsRunning());
                break;

            
        }
    }
}

void ButtonIdle()
{
    static int brightness = 255;
    static unsigned long lastStep = 0;
    const unsigned long stepInterval = 1; // ms entre cada passo ~2 segundos total

    esp_task_wdt_reset(); // ← sempre reseta em cada chamada

    bool idle = button[0]->getIdle() && button[1]->getIdle() && 
                button[2]->getIdle() && button[3]->getIdle();

    if (idle)
    {
        esp_task_wdt_reset(); // ← mantém watchdog feliz em idle
        if (brightness > 0 && millis() - lastStep > stepInterval) {
            lastStep = millis();
            brightness--;
            ledcWrite(0, brightness);
        }

        for (int i = 0; i < 4; i++) {
            if (button[i]->read()) break;
        }

        if (brightness == 0) 
        {
            menu = -2;
            
            sendAndReceiveData();
            
    
            esp_task_wdt_reset();
            wifi.loop();
        }
    }
    else
    {
        // fade up não bloqueante
        if (brightness < 255 && millis() - lastStep > stepInterval) {
            lastStep = millis();
            brightness++;
            ledcWrite(0, brightness);
            //menu = -2;
        }
    }
}




void initFirebaseStructure()
{
    if(wifi.getStatus() == false)
        return;
    String status = "";
    String version = ota.getVersion();
    firebase->awaitGet(safeEmail + "/Status", &status);
    firebase->aSyncSetString(safeEmail + "/Version", version);
    //Serial.printf("[Firebase] Status retornado: '%s'\n", status.c_str());
    
    if (status == "true" || status == "false")
    {
        display->logoScreen("Dados existentes carregados!");
        delay(500);
        return;
    }

    display->logoScreen("Primeiro acesso, criando estrutura...");
    esp_task_wdt_reset();

    // Status e email
    bool running = false;
    firebase->aSyncSetBool(safeEmail + "/Status", running);
    
    String emailStr = wifi.getEmail();
    firebase->aSyncSetString(safeEmail + "/email", emailStr);

    // Versão
    String ver = ota.getVersion();
    firebase->aSyncSetString(safeEmail + "/Version", ver);
    
    // Luz
    String hourOn = "08:00";
    String hourOff = "22:00";
    firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOn", hourOn);
    firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOff", hourOff);
    esp_task_wdt_reset();
    
    // Temperatura
    float targetTemp = data_class.getTargetTemp();
    float tempTol = data_class.getTempTolerance();
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp", targetTemp);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", tempTol);
    
    // Umidade
    float targetHumid = data_class.getTargetHumid();
    float humidTol = data_class.getHumidTolerance();
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid", targetHumid);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance", humidTol);
    esp_task_wdt_reset();
    
    // Solo
    float targetSoil = data_class.getTargetSoil();
    float soilTol = data_class.getSoilTolerance();
    float pumpDur = data_class.getPumpDuration();
    float absDelay = data_class.getAbsorptionDelay();
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil", targetSoil);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance", soilTol);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", pumpDur);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", absDelay);
    esp_task_wdt_reset();

    // Calibração solo — faltava
    float soilDry = data_class.getSoilLow();
    float soilWet = data_class.getSoilUpper();
    float pumpFlow = data_class.getPumpFlow();
    float behavior = data_class.getSoilBehavior();
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry", soilDry);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet", soilWet);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow", pumpFlow);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Behavior", behavior);
    esp_task_wdt_reset();

    // Reservatório — faltava
    float reserv = data_class.getWaterRawReading();
    float capacity = data_class.getWaterCapacity();
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Reserv", reserv);
    firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Capacity", capacity);

    display->logoScreen("Estrutura criada com sucesso!");
    delay(1000);
}

void wdt_conf()
{
 // Desabilita watchdog da task idle
    esp_task_wdt_deinit();
    
    // Reinicializa com timeout de 30 segundos
    esp_task_wdt_init(30, false); // 30s, não entra em panic
    esp_task_wdt_add(NULL);       // adiciona a task atual
}

void firebase_healthy_startup()
{
    if(firebase->init() && firebase->isReady() && firebase->isHealthy())
{
    const int MAX_ATTEMPTS = 3;
    const unsigned long OP_TIMEOUT = 20000; // 20s por operação

    bool structureOk = false;
    bool receiveOk   = false;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++)
    {
        //Serial.printf("[BOOT] Tentativa %d/%d de init Firebase\n", attempt, MAX_ATTEMPTS);
        esp_task_wdt_reset();

        // --- initFirebaseStructure com timeout ---
        if (!structureOk)
        {
            unsigned long t = millis();
            display->logoScreen("Carregando estrutura... (" + String(attempt) + "/" + String(MAX_ATTEMPTS) + ")");
            initFirebaseStructure();
            esp_task_wdt_reset();

            if (millis() - t < OP_TIMEOUT)
            {
                structureOk = true;
                //Serial.println("[BOOT] initFirebaseStructure OK");
            }
            else
            {
                //Serial.println("[BOOT] initFirebaseStructure TIMEOUT");
                continue; // tenta de novo
            }
        }

        // --- receiveFirebaseData com timeout ---
        if (structureOk && !receiveOk)
        {
            unsigned long t = millis();
            display->logoScreen("Recebendo dados... (" + String(attempt) + "/" + String(MAX_ATTEMPTS) + ")");
            receiveFirebaseData();
            esp_task_wdt_reset();

            if (millis() - t < OP_TIMEOUT)
            {
                receiveOk = true;
                //Serial.println("[BOOT] receiveFirebaseData OK");
            }
            else
            {
                //Serial.println("[BOOT] receiveFirebaseData TIMEOUT");
                continue;
            }
        }

        if (structureOk && receiveOk)
            break;
    }

    if (!structureOk || !receiveOk)
    {
        //Serial.println("[BOOT] Firebase falhou após todas as tentativas, reiniciando...");
        display->logoScreen("Falha na inicializacao, reiniciando...");
        delay(2000);
        ESP.restart();
    }

        display->logoScreen("Conectado ao banco de dados!");
    }
}

void heapMonitor()
{
    
    
    static unsigned long lastHeapLog = 0;
    if (millis() - lastHeapLog > 60000) 
    {
        lastHeapLog = millis();
        Serial.printf("[HEAP] Free: %d | Min ever: %d | Max block: %d\n",
            ESP.getFreeHeap(),
            ESP.getMinFreeHeap(),
            ESP.getMaxAllocHeap());
    }
}

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
    display->logoScreen("Ajustando protocolos de comunicacao...");
    delay(500);
    Serial2.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
    Serial2.flush();
    
   
    display->logoScreen("Inicializando equipamento...");
    delay(500);
    light.setTimeFunction(getNow);
    

    wifi.ensureCredentialsLoaded();

    if(wifi.checkCredentials())
    {
        display->logoScreen("Tentando conectar a: " + wifi.getSSID());
        if(wifi.wifiInit())
        {
            rtc.begin();
            
            display->logoScreen("Conectado a internet!");
            delay(500);
            display->logoScreen("Conectando ao banco de dados! Aguarde!");
        }

        safeEmail = wifi.getEmail();
        safeEmail.replace(".","_");
        
        firebase_healthy_startup();
    }
    else
    {
        display->logoScreen("Internet nao configurada, prosseguindo para menu de configuracao");
        menu = 6;
    }

    if(!wifi.getStatus() && !rtc.getStatus())
        display->logoScreen("Iniciando! Aguarde! Internet nao conectada, sem relogio!");
    else
        display->logoScreen("Iniciando! Aguarde!");

    sendDataStartUP();
    
    display->logoScreen("Versao: " + ota.getVersion());
    delay(1000);
    ota.fetchReleaseInfo();
    
  
}

void loop() 
{
    esp_task_wdt_reset();
    //heapMonitor();

    if(wifi.getStatus() && rtc.getStatus())
        getNow();
    
    ButtonIdle();
        
           
    unsigned long serialStart = millis();
    while(Serial2.available() >= 5 && millis() - serialStart < 50) {
        readPacket(&data_class);
        //feedWatchdog();
    }
    
    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    sendPacket(LIGHT0, lightValue);
    
    light.run(data_class.getIsRunning());

    static int last_menu = -2;

    if (menu < -1)
        menu = -1;

    if(last_menu != menu)
    {
        display->drawBackGround();
        last_menu = menu;
    }

           
    switch (menu)
    {
        case -1:

            display->mainScreen(&menu);
            break;
        
        case 0:

            display->monitorMenu(&menu);
            break;
    
        case 1:
        
            display->lightMenu(&menu);
            break;
    
        case 2:
            
            display->tempMenu(&menu); 
            break;
        
        case 3:
            
            display->humidMenu(&menu);
            break;
        
        case 4:
            if(data_class.getSoilBehavior() == SENSOR)
                display->soilMenuSensor(&menu);
            if(data_class.getSoilBehavior() == TIMER)
                display->soilMenuTimer(&menu);
            break;
        
        case 5:

            display->calibrationScreen(&menu);            
            break;
        
        case 6:
            
            display->setupScreen(&menu);
            break;

        default:
            break;
    }

}