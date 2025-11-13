#include "Libraries.hpp"


#include "WifiManager.hpp"
#include "Time.hpp"
#include "FBase.hpp"
#include "Display.hpp"
#include "Button.hpp"

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
CFastLED *led = NULL;


//Classes
WifiManager *wifi = NULL;
Time *rtc = NULL;
FBase *firebase = NULL;
Display *display = NULL;
Button *button = NULL;








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
    led = new CFastLED();
}

void initClasses()
{
	wifi = new WifiManager(&webServer, &prefs, &dnsServer, &wifiClient, &wifiClass, &display, &led);
	rtc = new Time(&display, "pool.ntp.org", "pool.ntp.br", -10800, 0);
	firebase = new FBase(&firebaseClient, API_KEY, DATABASE_URL, wifi->getEmail(), wifi->getPass(), &wifiClientSecure, &display, &led);
	display = new Display(&tft, &firebase, &rtc, &update, &wifi, &button);
}




void setup() 
{
	initDrivers();
	initClasses();


}

void loop() 
{



}