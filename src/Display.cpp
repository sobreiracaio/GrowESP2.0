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

void Display::connectionScreen(String note, String note1)
{
    String label = "Inicializando...";
    String foot = "Aguarde";
    
    int labelSize = display->textWidth(label, 4);
    int footSize = display->textWidth(foot, 4);
    int noteSize = display->textWidth(note, 2);
    int note1Size = display->textWidth(note1, 2);

    display->drawBitmap(150, 43, logoSmall, 180, 192, WHITE);
 
    display->setTextColor(WHITE, BLACK);
    display->drawString(label, (480 - labelSize) / 2, 20, 4);
    display->drawString(foot, (480 - footSize) / 2, 230, 4);
    display->drawString(note, (480 - noteSize) / 2, 260, 2);
    display->drawString(note1, (480 - note1Size) / 2, 280, 2);

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

void Display::flushScreen()
{
    display->fillScreen(BLACK);
}