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

//Classes
Light light;
WifiManager wifi(&prefs, nullptr);
Time rtc("pool.ntp.org", "pool.ntp.br", -10800, 0);
FBase *firebase = NULL;
Display *display = NULL;
Button* button[4] = {NULL};
DataClass data_class(&light);
OTA ota(&prefs, firebase, display);

struct tm now = {0};
String safeEmail = "";

float menu = 1;
bool initDevice = false;
bool isOTA = false;

// Estados do Firebase
enum FirebaseState {
    FB_STOPPED,           // Completamente desligado
    FB_STARTING,          // Iniciando
    FB_READY_FULL,        // Ativo para leitura/escrita (tela apagada)
    FB_READY_LIGHT        // Ativo apenas para escrita (adjustScreen)
};

static FirebaseState firebase_state = FB_STOPPED;
static unsigned long lastFirebaseReconnect = 0;
static hw_timer_t *watchdog = NULL;

// ============ WATCHDOG ============
void IRAM_ATTR resetModule() {
    ets_printf("WATCHDOG: Sistema travado! Reiniciando...\n");
    esp_restart();
}

void initWatchdog() {
    watchdog = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1MHz)
    timerAttachInterrupt(watchdog, &resetModule, true);
    timerAlarmWrite(watchdog, 120000000, false); // 120 segundos em microsegundos
    timerAlarmEnable(watchdog);
}

void feedWatchdog() {
    timerWrite(watchdog, 0); // Reseta o contador
}

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

void fireBaseLoadData(bool isOnLoop);

void initModules() {
    display->initDisplay();
    display->initLogoScreen();
    wifi.wifiInit();
    display->connectionScreen("Atualizando relogio", "     Aguarde...     ");
    rtc.begin();
    display->flushScreen();
    
    for (int i = 0; i < 4; i++)
        button[i]->init();
    
    safeEmail = wifi.getEmail();
    safeEmail.replace(".","_");
    firebase = new FBase(API_KEY, DATABASE_URL, wifi.getEmail(), wifi.getPass());
    display->injectFBase(firebase);
    ota.injectFbase(firebase);
    ota.injectDisplay(display);
    
    display->connectionScreen("Atualizando banco de dados", "     Aguarde...     ");
    
    unsigned long initStart = millis();
    while (!firebase->init()) {
        feedWatchdog(); // Alimenta watchdog durante init
        display->connectionScreen("Atualizando banco de dados", "     Aguarde...     ");
        
        if (millis() - initStart > 30000) { // Timeout de 30s
            display->connectionScreen("Erro na inicializacao", "Reiniciando...");
            delay(2000);
            ESP.restart();
        }
    }
    
    firebase->awaitSet(safeEmail + "/Pass", wifi.getPass(), STRING);
    feedWatchdog();
    fireBaseLoadData(false);
    firebase->stopApp();
    firebase_state = FB_STOPPED;
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

void getValues() {
    auto safeGet = [](String path, String* result, int delayMs = 200) {
        firebase->awaitGet(path, result);
        
        unsigned long start = millis();
        while (millis() - start < delayMs) {
            yield();
            feedWatchdog(); // Alimenta durante delays
            wifi.loop();
        }
    };

    String dayTime = "";
    String nightTime = "";
    String targetTemp = "";
    String tempTol = "";
    String targetHumid = "";
    String humidTol = "";
    String targetSoil = "";
    String pumpDur = "";
    String absDelay = "";
    String soilTol = "";
    String status = "";
    String binaryUrl = "";
    String token = "";
    String hasOTA = "";

    safeGet("/_Binary", &binaryUrl);
    ota.setBinaryPath(binaryUrl);
   
    safeGet("/_Token", &token);
    ota.setToken(token);

    safeGet("/_HasUpdate", &hasOTA);
    ota.setHasUpdate(hasOTA == "true" ? true : false);
    
    safeGet(safeEmail + "/InsertedData/Light/HourOn", &dayTime);
    formatDate(&dayTime, DAY);

    safeGet(safeEmail + "/InsertedData/Light/HourOff", &nightTime);
    formatDate(&nightTime, NIGHT);

    safeGet(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp", &targetTemp);
    data_class.setTargetTemp(targetTemp.toFloat());

    safeGet(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", &tempTol);
    data_class.setTempTolerance(tempTol.toFloat());

    safeGet(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid", &targetHumid);
    data_class.setTargetHumid(targetHumid.toFloat());

    safeGet(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance", &humidTol);
    data_class.setHumidTolerance(humidTol.toFloat());

    safeGet(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil", &targetSoil);
    data_class.setTargetSoil(targetSoil.toFloat());

    safeGet(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", &pumpDur);
    data_class.setPumpDuration(pumpDur.toFloat());

    safeGet(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", &absDelay);
    data_class.setAbsorptionDelay(absDelay.toFloat());

    safeGet(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance", &soilTol);
    data_class.setSoilTolerance(soilTol.toFloat());

    safeGet(safeEmail + "/Status", &status);
    data_class.setIsRunning(status == "true" ? true : false);
}

void fireBaseLoadData(bool isOnLoop) {
    if(isOnLoop) {
        const unsigned long INTERVAL_MS = 30000;
        static unsigned long lastSend = 0;
        unsigned long now = millis();

        if (now - lastSend < INTERVAL_MS) {
            return;
        }

        if (firebase_state != FB_READY_FULL) {
            lastSend = millis();
            return;
        }

        String needChange = "";
        firebase->awaitGet(safeEmail + "/needChange", &needChange);
        feedWatchdog();
        
        if(needChange == "true") {
            bool state = false;
            getValues();
            firebase->aSyncSetBool(safeEmail + "/needChange", state);
        }
        lastSend = millis();
        return;
    }

    getValues();
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

// ============ GERENCIAMENTO DE ESTADO DO FIREBASE ============
void stopFirebase() {
    if (firebase_state == FB_STOPPED) return;
    
    unsigned long stopStart = millis();
    firebase->stopApp();
    
    // Timeout de segurança
    while (millis() - stopStart < 2000) {
        yield();
        feedWatchdog();
    }
    
    firebase_state = FB_STOPPED;
}

bool startFirebase() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    unsigned long initStart = millis();
    bool success = firebase->init();
    
    // Se demorou mais de 5s, aborta
    if (millis() - initStart > 5000) {
        firebase->stopApp();
        return false;
    }
    
    return success;
}

void checkActivity(bool isOTA) {
    if(isOTA) return;

    bool allIdle = button[0]->getIdle() && button[1]->getIdle() &&
                   button[2]->getIdle() && button[3]->getIdle();

    static bool screenFadingOut = false;
    
    // ========== BOTÕES OCIOSOS (tela apagando) ==========
    if(allIdle && initDevice) {
        if (!screenFadingOut) {
            screenFadingOut = true;
        }
        
        if (display->fadeScreenOff()) {
            // Tela completamente apagada
            
            // Menu 2 (adjustScreen): Firebase modo LIGHT (apenas escrita)
            if (menu == 2) {
                if (firebase_state == FB_STOPPED) {
                    if (startFirebase()) {
                        firebase_state = FB_READY_LIGHT;
                    }
                } else if (firebase_state == FB_READY_FULL) {
                    // Downgrade para modo LIGHT
                    firebase_state = FB_READY_LIGHT;
                }
            }
            // Outros menus: Firebase modo FULL (leitura + escrita)
            else {
                if (firebase_state == FB_STOPPED) {
                    static unsigned long lastInitAttempt = 0;
                    unsigned long now = millis();
                    
                    // Tenta iniciar a cada 10s
                    if (now - lastInitAttempt > 10000) {
                        lastInitAttempt = now;
                        
                        if (startFirebase()) {
                            firebase_state = FB_READY_FULL;
                        }
                    }
                } else if (firebase_state == FB_READY_LIGHT) {
                    // Upgrade para modo FULL
                    firebase_state = FB_READY_FULL;
                }
            }
            
            // Envia dados apenas se Firebase estiver ativo
            if (firebase_state == FB_READY_FULL && firebase->isReady()) {
                display->asyncSet();
            }
        }
        
        menu = 0;
    }
    // ========== BOTÕES ATIVOS (tela acendendo) ==========
    else if(!allIdle) {
        screenFadingOut = false;
        
        // Desliga Firebase IMEDIATAMENTE quando botões são pressionados
        // EXCETO no menu 2 (adjustScreen) onde mantemos modo LIGHT
        if (menu != 2 && firebase_state != FB_STOPPED) {
            stopFirebase();
        }
        // No menu 2, mantém LIGHT se já estava ativo
        else if (menu == 2 && firebase_state == FB_READY_FULL) {
            firebase_state = FB_READY_LIGHT;
        }
        
        display->fadeScreenOn();
    }
}

void setup() {
    //Serial.begin(115200);
    
    // ============ INICIA WATCHDOG ============
    initWatchdog();
    
    Serial2.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
    initClasses();
    
    feedWatchdog(); // Alimenta antes de operações longas
    initModules();
    
    light.setTimeFunction(getNow);

    initDevice = true;
    
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
        sendPacket(arrID[i], arrVal[i]);
        delay(500);
        feedWatchdog(); // Alimenta durante envios
    }
    
    display->flushScreen();
}

void loop() {
    feedWatchdog(); // ============ ALIMENTA WATCHDOG NO INÍCIO ============
    
    checkActivity(isOTA);
    Serial.println(ESP.getFreeHeap());
    
    display->menuSwitch(&menu);
    
    // ========== GERENCIAMENTO WIFI + FIREBASE ==========
    if (WiFi.status() == WL_CONNECTED) {
        wifi.loop();
        
        // Firebase loop APENAS se estiver ativo
        if ((firebase_state == FB_READY_FULL || firebase_state == FB_READY_LIGHT) && firebase->isReady()) {
            firebase->loop();
        }
        // Reconexão automática se perdeu conexão
        else if ((firebase_state == FB_READY_FULL || firebase_state == FB_READY_LIGHT) && !firebase->isReady()) {
            static unsigned long lastReconnectAttempt = 0;
            unsigned long now = millis();
            
            if (now - lastReconnectAttempt > 30000) {
                lastReconnectAttempt = now;
                
                stopFirebase();
                delay(1000);
                feedWatchdog();
                
                if (startFirebase()) {
                    firebase_state = (menu == 2) ? FB_READY_LIGHT : FB_READY_FULL;
                }
            }
        }
    } else {
        wifi.loop();
        
        // Se WiFi caiu, desliga Firebase
        if (firebase_state != FB_STOPPED) {
            stopFirebase();
        }
    }
    
    getNow();

    // ========== PROCESSA SERIAL COM TIMEOUT ==========
    unsigned long serialStart = millis();
    while(Serial2.available() >= 5 && millis() - serialStart < 50) {
        readPacket(&data_class);
        feedWatchdog(); // Alimenta durante processamento serial
    }

    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    sendPacket(LIGHT0, lightValue);
    
    light.run(data_class.getIsRunning());
    
    fireBaseLoadData(true);
    
    feedWatchdog(); // ============ ALIMENTA WATCHDOG NO FINAL ============
}