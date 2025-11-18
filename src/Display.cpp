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
    topScreen("Painel Principal");
    bottomScreen("-", "+", "OK", "Cancel");

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

void Display::bottomScreen(String t1, String t2, String t3, String t4)
{
    display->setTextColor(WHITE, BLACK);

    String text[4] = {t1, t2, t3, t4};
    int colors[4] = {WHITE, WHITE, GREEN, RED};
    int space = 480 / 4; // 120 px
    int fontSize = 4;
    int fontHeight = 16; // altura aproximada da fonte 2
    int triHeight = 15;   // altura do triângulo
    int triWidth = 40;   // largura da base do triângulo

    for(int i = 0; i < 4; i++)
    {
        int xSpace = i * space;
        int textW = display->textWidth(text[i], fontSize);
        int xStart = xSpace + (space - textW)/2;
		display->setTextDatum(TL_DATUM);

        // desenha o texto
        display->drawString(text[i], xStart, 270, fontSize);

        // centro horizontal do texto
        int xCenter = xStart + textW/2;
        int distance = 280;

        // coordenadas do triângulo
        int x0 = xCenter - triWidth/2;
        int y0 = distance + fontHeight; // base do triângulo (logo abaixo do texto)
        int x1 = xCenter + triWidth/2;
        int y1 = distance + fontHeight;
        int x2 = xCenter;
        int y2 = distance + fontHeight + triHeight; // ponta do triângulo

        display->fillTriangle(x0, y0, x1, y1, x2, y2, colors[i]);
    }

}

void Display::topScreen(String label)
{
    const int height = display->fontHeight(2);
    const int textPos = (50 - height) / 2;
    const int sectionWidth = 440;
    const int textW = display->textWidth(label, 4);
    
    
	display->setTextDatum(MC_DATUM);
    display->setTextColor(WHITE, BLACK);
    
    
    int xStart = (sectionWidth) / 2;
    display->drawString(label, xStart, textPos, 4);
    

    String time = String(rtc->getHour()) + ":" + (rtc->getMinute() < 10 ? "0" + String(rtc->getMinute()) : String(rtc->getMinute()));
    
    const int timeX = 440; 

    display->setTextColor(WHITE, BLACK);
    display->drawString(time, timeX, textPos, 4);
}

void Display::animateArrow(int x, int y, int size, uint16_t color, int orientation)
{
    static unsigned long lastTime = 0;
    static int brightness = 0;
    static int direction = 1; // 1 = fade in, -1 = fade out

    unsigned long now = millis();

    // Velocidade da animação (ms entre frames)
    if (now - lastTime >= 20)
    {
        lastTime = now;

        brightness += direction * 10;

        if (brightness >= 255) {
            brightness = 255;
            direction = -1;
        }
        else if (brightness <= 0) {
            brightness = 0;
            direction = 1;
        }
    }

    // Converte cor original para RGB
    uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
    uint8_t b = (color & 0x1F) * 255 / 31;

    // Aplica brilho (fade)
    r = (r * brightness) / 255;
    g = (g * brightness) / 255;
    b = (b * brightness) / 255;

    uint16_t fadeColor = display->color565(r, g, b);

    // Desenha triângulo apontando para a direita
    if (orientation == LEFT)
    {
        display->fillTriangle(
        x,       y,          // ponto esquerdo
        x+size,  y-size,     // topo
        x+size,  y+size,     // base
        fadeColor
    );
    }
    
    if (orientation == RIGHT)
    {
        display->fillTriangle(
        x+size,  y,        // ponta direita
        x,       y-size,   // topo
        x,       y+size,   // base
        fadeColor
    );
    }
    
}
