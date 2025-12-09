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
    String binaryPath = "/_Binary";
    String token = "";
    String tokenPath = "/_Token";

    firebase->awaitGet(binaryPath, &binaryUrl);
    ota.setBinaryPath(binaryUrl);
   

    firebase->awaitGet(tokenPath, &token);
    ota.setToken(token);
    

    firebase->awaitGet(safeEmail + "/InsertedData/Light/HourOn", &dayTime);
    formatDate(&dayTime, DAY);

    firebase->awaitGet(safeEmail + "/InsertedData/Light/HourOff", &nightTime);
    formatDate(&nightTime, NIGHT);

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp", &targetTemp);
    data_class.setTargetTemp(targetTemp.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", &tempTol);
    data_class.setTempTolerance(tempTol.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid", &targetHumid);
    data_class.setTargetHumid(targetHumid.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance", &humidTol);
    data_class.setHumidTolerance(humidTol.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil", &targetSoil);
    data_class.setTargetSoil(targetSoil.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", &pumpDur);
    data_class.setPumpDuration(pumpDur.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", &absDelay);
    data_class.setAbsorptionDelay(absDelay.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance", &soilTol);
    data_class.setSoilTolerance(soilTol.toFloat());

    firebase->awaitGet(safeEmail + "/Status", &status);
    if (status == "true")
        data_class.setIsRunning(true);
    else
        data_class.setIsRunning(false);
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

        String needChange = "";
        firebase->awaitGet(safeEmail + "/needChange", &needChange);
        if(needChange == "true")
        {
            Serial1.end();
            bool state = false;
            getValues();
            firebase->aSyncSetBool(safeEmail + "/needChange", state);
            Serial1.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
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
    if(isOTA == true)
    {
        return;
    }
    if(button[0]->getIdle() && button[1]->getIdle() && button[2]->getIdle() && button[3]->getIdle() && initDevice)
        {
            if (display->fadeScreenOff())
            {
                if(firebase_running)
                {
                    if (firebase->init())
                        firebase_running = false;
                }
            }
            
            display->asyncSet();
            
            menu = 0;
           
        }
        else if(!button[0]->getIdle() || !button[1]->getIdle() || !button[2]->getIdle() || !button[3]->getIdle())
        {
            firebase_running = true;
            firebase->stopApp();
            display->fadeScreenOn();
        }
}


void setup() 
{
	//Serial.begin(115200);
	Serial1.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
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
  

	display->menuSwitch(&menu);
    wifi.loop();
    firebase->loop();
	getNow();

    while(Serial1.available() >= 5) {
        int result = readPacket(&data_class);
        if (result < 0) {
            //Serial.printf("❌ Erro ao ler pacote do Pico: %d\n", result);
        }
    }

    float lightValue = data_class.getIsRunning() ? light.getStatus() : 0;
    sendPacket(LIGHT0, lightValue);
    
   
    light.run(data_class.getIsRunning());
    
    
   
    
   


    
   
   
	
    fireBaseLoadData(true);
   
	
	
}