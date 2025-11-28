#include "Libraries.hpp"


#include "WifiManager.hpp"
#include "Time.hpp"
#include "FBase.hpp"
#include "Display.hpp"
#include "Button.hpp"
#include "DataClass.hpp"
#include "OTA.hpp"
#include "Light.hpp"

//Libraries
WiFiClass *wifiClass = NULL;
WiFiClientSecure *wifiClientSecure = NULL;
WiFiClient *wifiClient = NULL;
HTTPClient *httpClient = NULL;
WebServer *webServer = NULL;
DNSServer *dnsServer = NULL;

Preferences *prefs = NULL;

FirebaseClient *firebaseClient = NULL;

UpdateClass *update = NULL;
TFT_eSPI *tft = NULL;



//Classes
WifiManager *wifi = NULL;
Time *rtc = NULL;
FBase *firebase = NULL;
Display *display = NULL;
Button* button[4] = {NULL};
DataClass *data_class = NULL;
OTA *ota = NULL;
Light *light = NULL;


struct tm now = {0};
String safeEmail = "";

float menu = 0;

void initDrivers()
{
	wifiClass = new WiFiClass();
	wifiClientSecure = new WiFiClientSecure();
    wifiClient = new WiFiClient();
	httpClient = new HTTPClient();
    webServer = new WebServer(80);
    dnsServer = new DNSServer();
    prefs = new Preferences();
	firebaseClient = new FirebaseClient();
    update = new UpdateClass();
    tft = new TFT_eSPI();
   
}

void initClasses()
{
	light = new Light();
	data_class = new DataClass(light);
	
	button[0] = new Button(BTN1);
	button[1] = new Button(BTN2);
	button[2] = new Button(BTN3);
	button[3] = new Button(BTN4);
	
	
	wifi = new WifiManager(webServer, prefs, dnsServer, wifiClient, wifiClass, nullptr);
	rtc = new Time("pool.ntp.org", "pool.ntp.br", -10800, 0);
	firebase = new FBase(firebaseClient, API_KEY, DATABASE_URL, wifi->getEmail(), wifi->getPass(), wifiClientSecure);
	ota = new OTA(prefs);
	display = new Display(tft, firebase, rtc, ota, wifi, button, data_class); 
	
	wifi->injectDisplay(display);
	
}

void initModules()
{
	display->initDisplay();
	display->initLogoScreen();
	wifi->wifiInit();
	display->connectionScreen("Atualizando relogio", "     Aguarde...     ");
	rtc->begin();
	display->flushScreen();
	for (int i = 0; i < 4; i++)
		button[i]->init();
    
    safeEmail = wifi->getEmail();
    safeEmail.replace(".","_");
    display->connectionScreen("Atualizando banco de dados", "     Aguarde...     ");
    firebase->init();
    firebase->awaitSet(safeEmail + "/Pass", wifi->getPass(), STRING);
	

}

void parseReceivedData(const char *receivedData)
{
    static char lastData[128] = "";

    if (strcmp(receivedData, lastData) == 0)
        return;

    // Cópia segura da nova string recebida
    strncpy(lastData, receivedData, sizeof(lastData) - 1);
    lastData[sizeof(lastData) - 1] = '\0';

    // Variáveis temporárias para cada sensor
    float temp = 0.0f, humid = 0.0f, soil = 0.0f;
    int light = 0, pump = 0, cooler = 0, heater = 0, humidifier = 0, dehumidifier = 0;

    // Formato esperado: "T:25.5 H:60.2 S:45.0 L:1 P:0 C:1 He:0 Hu:1 DHu:0"
    int parsed = sscanf(receivedData,
                        "T:%f H:%f S:%f L:%d P:%d C:%d H:%d Hu:%d DHu:%d",
                        &temp, &humid, &soil,
                        &light, &pump, &cooler, &heater, &humidifier, &dehumidifier);

    if (parsed >= 3)
    {
        data_class->setTemp(temp);
        data_class->setHumid(humid);
        data_class->setSoil(soil);

        if (parsed >= 4) data_class->setLightStatus(light);
        if (parsed >= 5) data_class->setPumpStatus(pump);
        if (parsed >= 6) data_class->setCoolerStatus(cooler);
        if (parsed >= 7) data_class->setHeaterStatus(heater);
        if (parsed >= 8) data_class->setHumidStatus(humidifier);
        if (parsed >= 9) data_class->setDehumidStatus(dehumidifier);
    }
}


String parseDataToSend()
{
  String res;
  res  = "TT: "   + String(data_class->getTargetTemp()) ;
  res += " TTol: " + String(data_class->getTempTolerance()) ;
  res += " TH: "   + String(data_class->getTargetHumid()) ;
  res += " HTol: " + String(data_class->getHumidTolerance()) ;
  res += " TS: "   + String(data_class->getTargetSoil()) ;
  res += " STol: " + String(data_class->getSoilTolerance()) ;
  res += " PD: "   + String(data_class->getPumpDuration()) ;
  res += " AD: "   + String(data_class->getAbsorptionDelay()) ;
  res += " L: "    + String(data_class->getIsRunning() ? light->getStatus() : false);
  res += " STATUS: " + String(data_class->getIsRunning());
  res += "\n";

  return res;
}

String readData()
{
    if (Serial1.available()) 
    {
        // Retorna o objeto String. O chamador é responsável por gerenciar a memória
        return Serial1.readStringUntil('\n');
    }
    // Retorna String vazia, não o ponteiro vazio, o que é mais seguro.
    return "";
}

void sendData(String data)
{
	Serial1.print(data);
}

void getNow()
{
  now.tm_hour = rtc->getHour();
  now.tm_min = rtc->getMinute();
  now.tm_sec = rtc->getSecond();
  now.tm_mday = rtc->getDay();
  now.tm_mon = rtc->getMonth();
  now.tm_year = rtc->getYear();
  rtc->checkSync();
}



void setup() 
{
	Serial.begin(115200);
	Serial1.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
	initDrivers();
	initClasses();
	initModules();
	light->setTimeFunction(getNow);



}



void loop() 
{
	getNow();
	sendData(parseDataToSend());
  	parseReceivedData(readData().c_str());
	Serial.print(readData());
	light->run(data_class->getIsRunning());
	
	display->menuSwitch(&menu);
	
}