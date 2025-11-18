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
	data_class = new DataClass();

	button[0] = new Button(BTN1);
	button[1] = new Button(BTN2);
	button[2] = new Button(BTN3);
	button[3] = new Button(BTN4);

	display = new Display(tft, nullptr, nullptr, nullptr, nullptr, *button, data_class); 
	
	wifi = new WifiManager(webServer, prefs, dnsServer, wifiClient, wifiClass, display);
	rtc = new Time(display, "pool.ntp.org", "pool.ntp.br", -10800, 0);
	firebase = new FBase(firebaseClient, API_KEY, DATABASE_URL, wifi->getEmail(), wifi->getPass(), wifiClientSecure, display);
	ota = new OTA(display);

	display->setFBase(firebase);
	display->setTime(rtc);
	display->setOTA(ota);
	display->setWifi(wifi);

}




void setup() 
{
	initDrivers();
	initClasses();


}

void loop() 
{



}