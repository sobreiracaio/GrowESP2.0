#pragma once

#include "Libraries.hpp"
#include "image.hpp"
#include "Time.hpp"

#define DARK_GREY 0x18e3
#define BLACK     0x18a3  // 0x1E1E1E -> RGB(30,30,30)
#define WHITE     0xe73c
#define YELLOW    0xbc69  // 0xDDB967 -> RGB(221,185,103)
#define BLUE      0x4c57  // 0x2176AE -> RGB(33,118,174)
#define RED       0xba69  // 0xFF715B -> RGB(255,113,91)
#define GREEN     0x4dea  // 0x1EA896 -> RGB(30,168,150)

#define RIGHT 0
#define LEFT 1


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
        Button **btn;
        DataClass *dataClass;
        
        void bottomScreen(String t1, String t2, String t3, String t4);
        void topScreen(String label);
        
        
        public:
        Display(TFT_eSPI *tft, FBase *fbase, Time *time, OTA *OTA, WifiManager *wifi_manager, Button **button, DataClass *data_class);
        bool initDisplay();
        void initLogoScreen();
        void connectionScreen(String note, String note1);
        void qrScreen();
        void mainScreen();
        void adjustScreen();
        void confScreen();
        
        void flushScreen();
       
        void animateArrow(int x, int y, int size, uint16_t color, int orientation);
};