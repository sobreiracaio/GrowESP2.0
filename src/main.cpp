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

}




void setup() 
{
	Serial.begin(115200);
	initDrivers();
	initClasses();
	initModules();
	


	//display->connectionScreen("Inicializando o dispositivo", "Aguarde...");
	//wifi->handleReset();
	//wifi->wifiInit();

}



void loop() 
{
	Serial.println(menu);
	
	// Serial.printf("Button 0: %d\n",button[0]->getState());
	// Serial.printf("Button 1: %d\n",button[1]->getState());
	// Serial.printf("Button 2: %d\n",button[2]->getState());
	// Serial.printf("Button 3: %d\n",button[3]->getState());
	
	display->menuSwitch(&menu);
	
}