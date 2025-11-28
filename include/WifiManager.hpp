#pragma once

#include "Libraries.hpp"
#include "Display.hpp"

class Display;

class WifiManager {

    private:
        WebServer *webServer;
        Preferences *prefs;
        DNSServer *dnsServer;
        WiFiClient *wifiClient;
        WiFiClass *wifi;
        Display *display;

        String ssid;
        String password;
        String email;
        String userpass;

       

    

    public:
        WifiManager(WebServer *web_server, Preferences *preferences, DNSServer *dns_server, WiFiClient *wifi_client, WiFiClass * wifi_class, Display *tft);
        bool wifiInit();
        void loop();
        void connectToWiFi();
        bool getStatus();
        void startPortal();
        void handleRoot();
        void handleSave();
        void handleReset();

        String generateNetWorkList();
        String getEmail();
        String getPass();

        void injectDisplay(Display *tft);
};