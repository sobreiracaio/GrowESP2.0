#include "Display.hpp"

Display::Display(TFT_eSPI *tft, FBase *fbase, Time *time, OTA *ota, WifiManager *wifi_manager, Button **button, DataClass *data_class) :
                    display(tft), firebase(fbase), rtc(time), ota(ota), wifi(wifi_manager), btn(button), dataClass(data_class){}

bool Display::initDisplay()
{
    display->init();
    display->setRotation(1);

    ledcAttachPin(TFT_BL, 0);
    ledcSetup(0, 5000, 8);
    ledcWrite(0, 255);

    display->fillScreen(BLACK);
    return true;
}

void Display::initLogoScreen()
{
    display->drawBitmap(130, 43, logo, 220, 235, WHITE);
    delay(3000);
    display->fillScreen(BLACK);
}

void Display::connectionScreen()
{

}

void Display::qrScreen()
{
    display->fillScreen(BLACK);
    display->setTextColor(WHITE, BLACK);
    display->drawString("Growbox", 15, 40, 4);
    display->drawString("Para  configurar  sua  rede   e", 15, 225, 2);
    display->drawString("credenciais, aponte seu celular", 15, 240, 2);
    display->drawString("para este QR Code.", 15, 255, 2);
    display->drawSmoothRoundRect(220, 40, 5, 3, 240, 240, DARK_GREY, DARK_GREY);
    display->drawBitmap(230, 50, wifiqr, 220, 220, WHITE);
}
void Display::mainScreen()
{

}
void Display::adjustScreen()
{

}
void Display::confScreen()
{

}