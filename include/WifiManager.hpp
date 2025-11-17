#pragma once

#include "Libraries.hpp"

class Display;

class WifiManager {

    private:
        WebServer *webServer;
        Preferences *prefs;
        DNSServer *dnsServer;
        WiFiClient *wifiClient;
        WiFiClass *wifi;

        String ssid;
        String password;
        String email;
        String userpass;


        Display *display;
     

    public:
        WifiManager(WebServer *web_server, Preferences *preferences, DNSServer *dns_server, WiFiClient *wifi_client, WiFiClass * wifi_class,
                    Display *tft);
        
        String getEmail();
        String getPass();
};