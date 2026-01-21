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

static FirebaseState firebase_state = FB_STOPPED;
static FirebaseState firebase_target_state = FB_STOPPED;
static unsigned long firebaseTransitionStart = 0;
static hw_timer_t *watchdog = NULL;

// ============ CONTROLE DE TASKS ============
TaskHandle_t firebaseTaskHandle = NULL;
SemaphoreHandle_t firebaseMutex = NULL;
volatile bool firebaseTaskRunning = false;
volatile bool showLoadingBox = false;

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

// ============ LOADING BOX ============
void drawLoadingBox(String message = "Aguarde...") {
    int boxW = 300;
    int boxH = 100;
    int boxX = (480 - boxW) / 2;
    int boxY = (320 - boxH) / 2;
    
    display->getDisplay()->fillRoundRect(boxX, boxY, boxW, boxH, 10, TFT_DARKGREY);
    display->getDisplay()->drawRoundRect(boxX, boxY, boxW, boxH, 10, TFT_WHITE);
    
    display->getDisplay()->setTextDatum(MC_DATUM);
    display->getDisplay()->setTextColor(TFT_WHITE, TFT_DARKGREY);
    display->getDisplay()->drawString(message, 240, 160, 4);
}

void clearLoadingBox() {
    int boxW = 300;
    int boxH = 100;
    int boxX = (480 - boxW) / 2;
    int boxY = (320 - boxH) / 2;
    
    display->getDisplay()->fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
}

// ============ TASK FIREBASE ============
void firebaseTask(void *parameter) {
    FirebaseState action = *(FirebaseState*)parameter;
    
    if (action == FB_STARTING) {
        // Inicializa Firebase
        unsigned long startTime = millis();
        bool success = false;
        
        while (millis() - startTime < 8000 && !success) { // 8s timeout
            if (xSemaphoreTake(firebaseMutex, portMAX_DELAY)) {
                success = firebase->init();
                xSemaphoreGive(firebaseMutex);
            }
            
            if (success) {
                firebase_state = firebase_target_state;
                break;
            }
            
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        
        if (!success) {
            firebase_state = FB_STOPPED;
        }
    }
    else if (action == FB_STOPPING) {
        // Para Firebase
        if (xSemaphoreTake(firebaseMutex, portMAX_DELAY)) {
            firebase->stopApp();
            xSemaphoreGive(firebaseMutex);
        }
        
        vTaskDelay(500 / portTICK_PERIOD_MS);
        firebase_state = FB_STOPPED;
    }
    
    showLoadingBox = false;
    firebaseTaskRunning = false;
    vTaskDelete(NULL);
}

// ============ GERENCIAMENTO FIREBASE ============
void stopFirebase() {
    if (firebase_state == FB_STOPPED || firebase_state == FB_STOPPING) return;
    if (firebaseTaskRunning) return;
    
    firebase_state = FB_STOPPING;
    firebaseTaskRunning = true;
    
    FirebaseState action = FB_STOPPING;
    xTaskCreatePinnedToCore(
        firebaseTask,
        "StopFirebase",
        8192,
        (void*)&action,
        1,
        &firebaseTaskHandle,
        0  // Core 0
    );
}

void startFirebase(FirebaseState targetState) {
    if (firebase_state == FB_STARTING || firebase_state == targetState) return;
    if (firebaseTaskRunning) return;
    if (WiFi.status() != WL_CONNECTED) return;
    
    firebase_state = FB_STARTING;
    firebase_target_state = targetState;
    firebaseTaskRunning = true;
    showLoadingBox = true;
    
    FirebaseState action = FB_STARTING;
    xTaskCreatePinnedToCore(
        firebaseTask,
        "StartFirebase",
        8192,
        (void*)&action,
        1,
        &firebaseTaskHandle,
        0  // Core 0
    );
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
        firebase->awaitGet(path, result);
        
        unsigned long start = millis();
        while (millis() - start < delayMs) {
            yield();
            feedWatchdog();
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
    
    // ========== DETECTA MUDANÇA DE MENU ==========
    if (menu != lastMenu && menu != 0) {
        // Mudou de menu
        if (menu == 2) {
            // Indo para adjustScreen - precisa do Firebase
            if (firebase_state == FB_STOPPED) {
                showLoadingBox = true;
                startFirebase(FB_READY_LIGHT);
            } else if (firebase_state == FB_READY_FULL) {
                firebase_state = FB_READY_LIGHT;
            }
        } else {
            // Saindo do adjustScreen ou mudando para outro menu
            if (firebase_state == FB_READY_LIGHT || firebase_state == FB_READY_FULL) {
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
        }
        
        if (display->fadeScreenOff()) {
            // Tela completamente apagada - inicia Firebase silenciosamente
            
            if (!firebaseRequested && firebase_state == FB_STOPPED) {
                firebaseRequested = true;
                showLoadingBox = false; // Sem loading box quando tela apagada
                startFirebase(FB_READY_FULL);
            }
            
            // Envia dados apenas se Firebase estiver completamente pronto
            if ((firebase_state == FB_READY_FULL || firebase_state == FB_READY_LIGHT) && 
                firebase->isReady() && xSemaphoreTake(firebaseMutex, 10 / portTICK_PERIOD_MS)) {
                display->asyncSet();
                xSemaphoreGive(firebaseMutex);
            }
        }
        
        menu = 0;
    }
    // ========== BOTÕES ATIVOS (tela acendendo) ==========
    else if(!allIdle) {
        screenFadingOut = false;
        firebaseRequested = false;
        
        // Para Firebase se não estiver no adjustScreen
        if (menu != 2 && firebase_state != FB_STOPPED && firebase_state != FB_STOPPING) {
            stopFirebase();
        }
        
        display->fadeScreenOn();
    }
}

void setup() {
    Serial.begin(115200);
    
    // ============ CRIA MUTEX ============
    firebaseMutex = xSemaphoreCreateMutex();
    
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
}

void loop() {
    feedWatchdog();
    
    checkActivity(isOTA);
    
    // ========== DESENHA LOADING BOX SE NECESSÁRIO ==========
    static bool lastLoadingState = false;
    if (showLoadingBox && !lastLoadingState) {
        drawLoadingBox("Conectando...");
        lastLoadingState = true;
    } else if (!showLoadingBox && lastLoadingState) {
        clearLoadingBox();
        lastLoadingState = false;
    }
    
    display->menuSwitch(&menu);
    
    // ========== GERENCIAMENTO WIFI + FIREBASE ==========
    if (WiFi.status() == WL_CONNECTED) {
        wifi.loop();
        
        // Firebase loop APENAS se estiver completamente ativo e não em transição
        if ((firebase_state == FB_READY_FULL || firebase_state == FB_READY_LIGHT) && 
            firebase->isReady() && !firebaseTaskRunning &&
            xSemaphoreTake(firebaseMutex, 10 / portTICK_PERIOD_MS)) {
            
            firebase->loop();
            xSemaphoreGive(firebaseMutex);
        }
    } else {
        wifi.loop();
        
        // Se WiFi caiu, desliga Firebase
        if (firebase_state != FB_STOPPED && firebase_state != FB_STOPPING && !firebaseTaskRunning) {
            stopFirebase();
        }
    }
    
    getNow();

    // ========== PROCESSA SERIAL COM TIMEOUT ==========
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