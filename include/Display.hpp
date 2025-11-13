#pragma once

#include "Libraries.hpp"


class WifiManager;
class Time;
class FBase;

class Button;

class Display {

    private:
        TFT_eSPI *display;

        FBase *firebase;
        Time *rtc;
        UpdateClass *update;
        WifiManager *wifi;
        Button *btn;
        


    public:
        Display(TFT_eSPI *tft, FBase *fbase, Time *time, UpdateClass *ota, WifiManager *wifi_manager, Button *button);

};