#pragma once

#include "Libraries.hpp"


class WifiManager;
class Time;
class FBase;
class Button;
class DataClass;
class OTA;

class Display {

    private:
        TFT_eSPI *display;

        FBase *firebase;
        Time *rtc;
        OTA *ota;
        WifiManager *wifi;
        Button *btn;
        DataClass *dataClass;
        


    public:
        Display(TFT_eSPI *tft, FBase *fbase, Time *time, OTA *OTA, WifiManager *wifi_manager, Button *button, DataClass *data_class);
        void setFBase(FBase *fbase);
        void setTime (Time *time);
        void setOTA (OTA *OTA);
        void setWifi (WifiManager *wifi_manager);
};