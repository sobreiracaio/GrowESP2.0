#pragma once

#include "Libraries.hpp"
#include "Display.hpp"

class Display;

class WifiManager {

    private:
        WebServer webServer{80};
        Preferences *prefs;
        DNSServer dnsServer;
        WiFiClient wifiClient;
        Display *display;

        String ssid;
        String password;
        String email;
        String userpass;

       

    

    public:
        WifiManager(Preferences *preferences, Display *tft);
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

        int getSignalStrenght();

        void injectDisplay(Display *tft);
};