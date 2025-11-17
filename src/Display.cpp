#include "Display.hpp"

Display::Display(TFT_eSPI *tft, FBase *fbase, Time *time, OTA *ota, WifiManager *wifi_manager, Button *button, DataClass *data_class) :
                    display(tft), firebase(fbase), rtc(time), ota(ota), wifi(wifi_manager), btn(button), dataClass(data_class){}

void Display::setFBase(FBase *fbase)
{
    firebase = fbase;
}
void Display::setTime (Time *time)
{
    rtc = time;
}
void Display::setOTA (OTA *OTA)
{
    ota = OTA;
}
void Display::setWifi (WifiManager *wifi_manager)
{
    wifi = wifi_manager;
}