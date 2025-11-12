#include "WifiManager.hpp"

WifiManager::WifiManager(WebServer **web_server, Preferences **preferences, DNSServer **dns_server, WiFiClient **wifi_client, WiFiClass ** wifi_class,
                         Display **tft, CFastLED **led) : webServer(web_server), prefs(preferences), dnsServer(dns_server),
                                                           wifiClient(wifi_client), wifi(wifi_class), display(tft), leds(led) 
                                                           {}

String WifiManager::getEmail()
{
    return email;
}
String WifiManager::getPass()
{
    return userpass;
}