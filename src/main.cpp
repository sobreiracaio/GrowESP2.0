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
    FB_STOPPED,           
    FB_STARTING,          
    FB_STOPPING,          
    FB_READY_FULL,        
    FB_READY_LIGHT        
};

volatile FirebaseState firebase_state = FB_STOPPED;
volatile FirebaseState firebase_target_state = FB_STOPPED;
volatile bool firebase_is_ready = false;
static hw_timer_t *watchdog = NULL;

// ============ CONTROLE DUAL-CORE ============
TaskHandle_t firebaseTaskHandle = NULL;
TaskHandle_t uiTaskHandle = NULL;
SemaphoreHandle_t firebaseMutex = NULL;
SemaphoreHandle_t displayMutex = NULL;

volatile bool showLoadingBox = false;
volatile bool needsLoadingRedraw = false;

// ============ WATCHDOG ============
void IRAM_ATTR resetModule() {
    ets_printf("WATCHDOG: Sistema travado! Reiniciando...\n");
    esp_restart();
}

void initWatchdog() {
    watchdog = timerBegin(0, 80, true);
    timerAttachInterrupt(watchdog, &resetModule, true);
    timerAlarmWrite(watchdog, 120000000, false);
    timerAlarmEnable(watchdog);
}

void feedWatchdog() {
    timerWrite(watchdog, 0);
}

// ============ LOADING BOX (CORE 1 APENAS) ============
void drawLoadingBox(String message = "Conectando...") {
    if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        int boxW = 300;
        int boxH = 100;
        int boxX = (480 - boxW) / 2;
        int boxY = (320 - boxH) / 2;
        
        display->getDisplay()->fillRoundRect(boxX, boxY, boxW, boxH, 10, TFT_DARKGREY);
        display->getDisplay()->drawRoundRect(boxX, boxY, boxW, boxH, 10, TFT_WHITE);
        
        display->getDisplay()->setTextDatum(MC_DATUM);
        display->getDisplay()->setTextColor(TFT_WHITE, TFT_DARKGREY);
        display->getDisplay()->drawString(message, 240, 160, 4);
        
        xSemaphoreGive(displayMutex);
    }
}

void clearLoadingBox() {
    if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        int boxW = 300;
        int boxH = 100;
        int boxX = (480 - boxW) / 2;
        int boxY = (320 - boxH) / 2;
        
        display->getDisplay()->fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
        
        xSemaphoreGive(displayMutex);
    }
}

// ============ TASK FIREBASE (CORE 0 - DEDICADO) ============
void firebaseCoreTask(void *parameter) {
    Serial.println("[CORE0] Firebase task iniciada");
    
    while(true) {
        feedWatchdog();
        
        // ========== GERENCIA ESTADOS DO FIREBASE ==========
        if (firebase_state == FB_STARTING && WiFi.status() == WL_CONNECTED) {
            Serial.println("[CORE0] Iniciando Firebase...");
            
            if (xSemaphoreTake(firebaseMutex, portMAX_DELAY)) {
                bool success = firebase->init();
                
                if (success && firebase->isReady()) {
                    firebase_state = firebase_target_state;
                    firebase_is_ready = true;
                    showLoadingBox = false;
                    Serial.println("[CORE0] Firebase pronto!");
                } else {
                    firebase->stopApp();
                    firebase_state = FB_STOPPED;
                    firebase_is_ready = false;
                    showLoadingBox = false;
                    Serial.println("[CORE0] Firebase falhou ao iniciar");
                }
                
                xSemaphoreGive(firebaseMutex);
            }
        }
        else if (firebase_state == FB_STOPPING) {
            Serial.println("[CORE0] Parando Firebase...");
            
            if (xSemaphoreTake(firebaseMutex, portMAX_DELAY)) {
                firebase->stopApp();
                firebase_state = FB_STOPPED;
                firebase_is_ready = false;
                showLoadingBox = false;
                xSemaphoreGive(firebaseMutex);
            }
            
            Serial.println("[CORE0] Firebase parado");
        }
        
        // ========== FIREBASE LOOP (quando ativo) ==========
        if ((firebase_state == FB_READY_FULL || firebase_state == FB_READY_LIGHT) && 
            firebase_is_ready && WiFi.status() == WL_CONNECTED) {
            
            if (xSemaphoreTake(firebaseMutex, 10 / portTICK_PERIOD_MS)) {
                firebase->loop();
                
                // Envia dados se necessário
                static unsigned long lastAsyncSend = 0;
                if (firebase_state == FB_READY_FULL && millis() - lastAsyncSend > 60000) {
                    display->asyncSet();
                    lastAsyncSend = millis();
                }
                
                xSemaphoreGive(firebaseMutex);
            }
        }
        
        // ========== WIFI LOOP ==========
        wifi.loop();
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// ============ COMANDOS PARA FIREBASE (chamados do Core 1) ============
void stopFirebase() {
    if (firebase_state == FB_STOPPED || firebase_state == FB_STOPPING) return;
    
    Serial.println("[CMD] Solicitando parada do Firebase");
    firebase_state = FB_STOPPING;
}

void startFirebase(FirebaseState targetState, bool showLoading = true) {
    if (firebase_state == FB_STARTING || firebase_state == targetState) return;
    if (WiFi.status() != WL_CONNECTED) return;
    
    Serial.printf("[CMD] Solicitando inicio do Firebase (target: %d)\n", targetState);
    firebase_target_state = targetState;
    firebase_state = FB_STARTING;
    showLoadingBox = showLoading;
    needsLoadingRedraw = showLoading;
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
        feedWatchdog();
        display->connectionScreen("Atualizando banco de dados", "     Aguarde...     ");
        
        if (millis() - initStart > 30000) {
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
        if (xSemaphoreTake(firebaseMutex, portMAX_DELAY)) {
            firebase->awaitGet(path, result);
            xSemaphoreGive(firebaseMutex);
        }
        
        unsigned long start = millis();
        while (millis() - start < delayMs) {
            yield();
            feedWatchdog();
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

        if (firebase_state != FB_READY_FULL || !firebase_is_ready) {
            lastSend = millis();
            return;
        }

        if (xSemaphoreTake(firebaseMutex, 100 / portTICK_PERIOD_MS)) {
            String needChange = "";
            firebase->awaitGet(safeEmail + "/needChange", &needChange);
            feedWatchdog();
            
            if(needChange == "true") {
                bool state = false;
                getValues();
                firebase->aSyncSetBool(safeEmail + "/needChange", state);
            }
            xSemaphoreGive(firebaseMutex);
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

void checkActivity(bool isOTA) {
    if(isOTA) return;

    bool allIdle = button[0]->getIdle() && button[1]->getIdle() &&
                   button[2]->getIdle() && button[3]->getIdle();

    static bool screenFadingOut = false;
    static bool firebaseRequested = false;
    static float lastMenu = -1;
    static bool screenFullyOff = false;
    static unsigned long menuChangeTime = 0;
    
    // ========== DETECTA MUDANÇA DE MENU ==========
    if (menu != lastMenu && menu != 0) {
        menuChangeTime = millis();
        Serial.printf("[UI] Mudanca de menu: %.0f -> %.0f\n", lastMenu, menu);
        
        if (menu == 2) {
            // Indo para adjustScreen - PRECISA do Firebase READY_LIGHT
            if (firebase_state != FB_READY_LIGHT || !firebase_is_ready) {
                Serial.println("[UI] Mostrando loading box para menu 2");
                showLoadingBox = true;
                needsLoadingRedraw = true;
                
                if (firebase_state == FB_STOPPED || firebase_state == FB_STOPPING) {
                    startFirebase(FB_READY_LIGHT, true);
                } else if (firebase_state == FB_READY_FULL && firebase_is_ready) {
                    firebase_state = FB_READY_LIGHT;
                    showLoadingBox = false;
                } else if (firebase_state == FB_STARTING) {
                    firebase_target_state = FB_READY_LIGHT;
                }
            }
        } else {
            // Saindo do adjustScreen
            if (firebase_state != FB_STOPPED) {
                stopFirebase();
            }
        }
        lastMenu = menu;
    }
    
    // ========== BOTÕES OCIOSOS (tela apagando) ==========
    if(allIdle && initDevice) {
        if (!screenFadingOut) {
            screenFadingOut = true;
            firebaseRequested = false;
            screenFullyOff = false;
        }
        
        bool fadeDone = display->fadeScreenOff();
        
        if (fadeDone && !screenFullyOff) {
            screenFullyOff = true;
        }
        
        if (screenFullyOff && !firebaseRequested && firebase_state == FB_STOPPED) {
            firebaseRequested = true;
            startFirebase(FB_READY_FULL, false);
        }
        
        menu = 0;
    }
    // ========== BOTÕES ATIVOS (tela acendendo) ==========
    else if(!allIdle) {
        screenFadingOut = false;
        firebaseRequested = false;
        screenFullyOff = false;
        
        if (menu != 2 && firebase_state != FB_STOPPED) {
            stopFirebase();
        }
        
        display->fadeScreenOn();
    }
}

void setup() {
    Serial.begin(115200);
    
    // ============ CRIA MUTEXES ============
    firebaseMutex = xSemaphoreCreateMutex();
    displayMutex = xSemaphoreCreateMutex();
    
    // ============ INICIA WATCHDOG ============
    initWatchdog();
    
    Serial2.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
    initClasses();
    
    feedWatchdog();
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
        feedWatchdog();
    }
    
    display->flushScreen();
    
    // ============ INICIA TASK DO FIREBASE NO CORE 0 ============
    xTaskCreatePinnedToCore(
        firebaseCoreTask,
        "FirebaseCore",
        16384,  // Stack maior para Firebase
        NULL,
        1,
        &firebaseTaskHandle,
        0  // CORE 0 dedicado ao Firebase
    );
    
    Serial.println("[SETUP] Task Firebase criada no Core 0");
}

void loop() {
    // ============ CORE 1 - UI E DISPLAY APENAS ============
    feedWatchdog();
    
    checkActivity(isOTA);
    
    // ========== LOADING BOX (redesenha se necessário) ==========
    if (needsLoadingRedraw) {
        drawLoadingBox("Conectando...");
        needsLoadingRedraw = false;
    }
    
    // Remove loading box quando Firebase estiver pronto
    if (showLoadingBox && firebase_state == FB_READY_LIGHT && firebase_is_ready) {
        clearLoadingBox();
        showLoadingBox = false;
    }
    
    // Não desenha menu enquanto loading box ativo
    if (!showLoadingBox) {
        display->menuSwitch(&menu);
    } else {
        // Redesenha loading box periodicamente
        static unsigned long lastRedraw = 0;
        if (millis() - lastRedraw > 500) {
            needsLoadingRedraw = true;
            lastRedraw = millis();
        }
    }
    
    getNow();

    // ========== PROCESSA SERIAL ==========
    unsigned long serialStart = millis();
    while(Serial2.available() >= 5 && millis() - serialStart < 50) {
        readPacket(&data_class);
        feedWatchdog();
    }

    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    sendPacket(LIGHT0, lightValue);
    
    light.run(data_class.getIsRunning());
    
    fireBaseLoadData(true);
    
    feedWatchdog();
}