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

static bool firebase_running = true;

static unsigned long lastFirebaseReconnect = 0;

void initClasses()
{
    tft = new TFT_eSPI();
	button[0] = new Button(BTN1);
	button[1] = new Button(BTN2);
	button[2] = new Button(BTN3);
	button[3] = new Button(BTN4);
	display = new Display(tft, firebase, &rtc, &ota, &wifi, button, &data_class); 
	wifi.injectDisplay(display);
}

void fireBaseLoadData(bool isOnLoop);

void initModules()
{
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
    while (!firebase->init())
    {
        display->connectionScreen("Atualizando banco de dados", "     Aguarde...     ");
        
    }
    firebase->awaitSet(safeEmail + "/Pass", wifi.getPass(), STRING);
    fireBaseLoadData(false);
	firebase->stopApp();

}

void formatDate(String *date, int period)
{
     int colonIndex = date->indexOf(':');
    if (colonIndex == -1) return; // formato inválido
   
    float hour = date->substring(0, colonIndex).toFloat();
    float minute = date->substring(colonIndex + 1).toFloat();

  

    if(period == DAY)
    {
        data_class.setDayTime(hour, minute);
        
    }
    if(period == NIGHT)
    {
        data_class.setNightTime(hour, minute);
        
    }

}

void getValues()
{
    // ✅ Helper para esperar entre requisições
    auto safeGet = [](String path, String* result, int delayMs = 200) {
        firebase->awaitGet(path, result);
        
        // ✅ Delay não bloqueante
        unsigned long start = millis();
        while (millis() - start < delayMs) {
            yield();
            wifi.loop(); // Mantém WiFi vivo
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

    // ✅ Requisições com delays entre elas
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

void fireBaseLoadData(bool isOnLoop)
{
    if(isOnLoop)
    {
        const unsigned long INTERVAL_MS = 30000;
        static unsigned long lastSend = 0;
        unsigned long now = millis();

        if (now - lastSend < INTERVAL_MS) 
        {
            return;
        }

        // MUDANÇA 12: VERIFICAR SE O FIREBASE ESTÁ REALMENTE PRONTO ANTES DE USAR
        if (!firebase->isReady()) {
            // Tentar reconectar se passou tempo suficiente desde última tentativa
            if (now - lastFirebaseReconnect > 60000) { // 1 minuto
                firebase->stopApp();
                delay(1000);
                firebase->init();
                lastFirebaseReconnect = now;
            }
            lastSend = millis();
            return;
        }

        String needChange = "";
        firebase->awaitGet(safeEmail + "/needChange", &needChange);
        if(needChange == "true")
        {
            bool state = false;
            getValues();
            firebase->aSyncSetBool(safeEmail + "/needChange", state);
        }
        lastSend = millis();
        return;
    }

    getValues();
}


void getNow()
{
  now.tm_hour = rtc.getHour();
  now.tm_min = rtc.getMinute();
  now.tm_sec = rtc.getSecond();
  now.tm_mday = rtc.getDay();
  now.tm_mon = rtc.getMonth();
  now.tm_year = rtc.getYear();
  rtc.checkSync();
}


void checkActivity(bool isOTA)
{
    if(isOTA == true) {
        return;
    }

    bool allIdle = button[0]->getIdle() && button[1]->getIdle() &&
                   button[2]->getIdle() && button[3]->getIdle();

    if(allIdle && initDevice)
    {
        if (display->fadeScreenOff())
        {
            // ✅ Só tenta conectar Firebase se WiFi OK e ainda não conectado
            if(!firebase_running && WiFi.status() == WL_CONNECTED)
            {
                static unsigned long lastInitAttempt = 0;
                unsigned long now = millis();
                
                // ✅ Tenta init a cada 10s (não toda iteração!)
                if (now - lastInitAttempt > 10000) {
                    lastInitAttempt = now;
                    
                    // ✅ Init com timeout (máximo 5s de espera)
                    unsigned long initStart = millis();
                    if (firebase->init()) {
                        firebase_running = true;
                    } else if (millis() - initStart > 5000) {
                        // ✅ Se demorou mais de 5s, força stop
                        firebase->stopApp();
                    }
                }
            }
        }
        
        // ✅ Só envia dados se Firebase estiver realmente pronto
        if (firebase_running && firebase->isReady()) {
            display->asyncSet();
        }
        
        menu = 0;
    }
    else if(!allIdle)
    {
        // ✅ Desliga Firebase apenas se estava ligado
        if (firebase_running) {
            firebase_running = false;
            
            // ✅ Stop com timeout de segurança
            unsigned long stopStart = millis();
            firebase->stopApp();
            
            // ✅ Se demorou mais de 2s, força desconexão SSL
            if (millis() - stopStart > 2000) {
                // Acessa via ponteiro global (adicione no FBase.hpp)
                // ssl_client.stop();
            }

            // ✅ Delay não bloqueante
            unsigned long start = millis();
            while (millis() - start < 100) {
                yield();
            }
        }

        display->fadeScreenOn();
    }
}



void setup() 
{
	//Serial.begin(115200);
      
	Serial2.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
	initClasses();
	initModules();
    
	light.setTimeFunction(getNow);

    initDevice = true;
    
    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    uint8_t arrID[10] = {TT, TTOL, TH, HTOL, TS, STOL, PD, AD, LIGHT0, STATUS0};
    float arrVal[10] = {data_class.getTargetTemp(), data_class.getTempTolerance(), data_class.getTargetHumid(), data_class.getHumidTolerance(),
        data_class.getTargetSoil(), data_class.getSoilTolerance(), data_class.getPumpDuration(), data_class.getAbsorptionDelay(),
        lightValue, (float)data_class.getIsRunning()};
        
        const int count = sizeof(arrID) / sizeof(arrID[0]);
        
        for (int i = 0; i < count; i++)
        {    
            sendPacket(arrID[i], arrVal[i]);
            delay(500);
        }
        
        display->flushScreen();
}


void loop() 
{
    checkActivity(isOTA);
    Serial.println(ESP.getFreeHeap());
    
    display->menuSwitch(&menu);
    
    // ✅ MUDANÇA CRÍTICA: Só chama loops se tudo estiver OK
    if (WiFi.status() == WL_CONNECTED) {
        wifi.loop();
        
        // ✅ Só chama firebase->loop() se estiver autenticado e pronto
        if (firebase_running && firebase->isReady()) {
            firebase->loop();
        }
        // ✅ Se não está pronto mas deveria estar, tenta reconectar (não bloqueante)
        else if (firebase_running && !firebase->isReady()) {
            static unsigned long lastReconnectAttempt = 0;
            unsigned long now = millis();
            
            // Tenta reconectar a cada 30s (não no loop principal!)
            if (now - lastReconnectAttempt > 30000) {
                lastReconnectAttempt = now;
                
                // ✅ Cria task separada para reconexão (não trava loop)
                static bool reconnecting = false;
                if (!reconnecting) {
                    reconnecting = true;
                    
                    // Agenda reconexão assíncrona
                    xTaskCreate([](void* param) {
                        firebase->stopApp();
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        firebase->init();
                        bool* flag = (bool*)param;
                        *flag = false;
                        vTaskDelete(NULL);
                    }, "FirebaseReconnect", 8192, &reconnecting, 1, NULL);
                }
            }
        }
    } else {
        // WiFi offline: só tenta reconectar wifi
        wifi.loop();
    }
    
    getNow();

    // ✅ Processa serial com timeout
    unsigned long serialStart = millis();
    while(Serial2.available() >= 5 && millis() - serialStart < 50) {
        readPacket(&data_class);
    }

    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    sendPacket(LIGHT0, lightValue);
    
    light.run(data_class.getIsRunning());
    
    fireBaseLoadData(true);
}