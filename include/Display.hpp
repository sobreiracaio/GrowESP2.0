#pragma once

#include "Libraries.hpp"
#include "image.hpp"
#include "Time.hpp"
#include "Button.hpp"
#include "DataClass.hpp"
#include "OTA.hpp"
#include "WifiManager.hpp"
#include "FBase.hpp"
#include "Serial.hpp"

#define DARK_GREY  0x18e3
#define LIGHT_GREY 0x6b4d
#define BLACK      0x18a3  // 0x1E1E1E -> RGB(30,30,30)
#define WHITE      0xe73c
#define YELLOW     0xbc69  // 0xDDB967 -> RGB(221,185,103)
#define BLUE       0x4c57  // 0x2176AE -> RGB(33,118,174)
#define RED        0xba69  // 0xFF715B -> RGB(255,113,91)
#define GREEN      0x4dea  // 0x1EA896 -> RGB(30,168,150)
#define PURPLE     0x701f 
#define ORANGE     0xdca6
#define BROWN      0x7363
#define BG_ICON    0x1122

#define RIGHT 0
#define LEFT 1
#define BOTH -1

extern String safeEmail;

class WifiManager;
class Time;
class FBase;
class Button;
class DataClass;
class OTA;

class Display {

    private:
        TFT_eSPI *display;
        TFT_eSprite *gaugeSprite = nullptr;

        uint16_t gradientCache[3][270];
        bool gradientCached = false;

        FBase *firebase;
        Time *rtc;
        OTA *ota;
        WifiManager *wifi;
        Button **btn;
        DataClass *dataClass;

        uint16_t pulseColorTimed(uint16_t c1, uint16_t c2, float speed);
        void drawArcGauge(int x, int y, float value, float targetValue, float ceiling_value, float tolerance, String label, String unit, int option = 0);
        
        void boxButton(int x, int y, int w, int h, int color, const unsigned char *image, bool is_selected = false, String text_top = "", String text_bot = "", int text_size = 2);
        void buttonScreen(String labels[2][4], int colors[2][4], const unsigned char *images[2][4], int *menu, int menu_nbr);
        void topScreen(String label = "");
        void botScreen(String labels[4]);
        void drawAntenna();
        void waitBox();

        
        
        
        public:
        Display(TFT_eSPI *tft, FBase *fbase, Time *time, OTA *OTA, WifiManager *wifi_manager, Button **button, DataClass *data_class);
        bool initDisplay();
        
        void injectFBase(FBase *fbase);

        void drawBackGround(int color = DARK_GREY);

        void logoScreen(String text);
        void mainScreen(int *menu);

        void monitorMenu(int *menu);
        void lightMenu(int *menu);
        void tempMenu(int *menu);
        void humidMenu(int *menu);
        void soilMenuSensor(int *menu);
        void soilMenuTimer(int *menu);

        void calibrationScreen(int *menu);
        void setupScreen(int *menu);

        void soilSensorCalibScreen(int *menu);
        void waterReservCalibScreen(int *menu);
        void pumpCalibScreen(int *menu);
        void wateringCalibScreen(int *menu);
        void lightCalibScreen(int *menu);

        void wifiConfScreen(int *menu);
        void clockConfScreen(int *menu);
        void updateConfScreen(int *menu);
        
        void aboutConfScreen(int *menu);

        void connectionScreen(String label_1, String label_2);
        void wifiConnStatus();
        void qrScreen();

        int day[2];
        int night[2];
        
        

};