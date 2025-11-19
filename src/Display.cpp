#include "Display.hpp"

Display::Display(TFT_eSPI *tft, FBase *fbase, Time *time, OTA *ota, WifiManager *wifi_manager, Button **button, DataClass *data_class) :
                    display(tft), firebase(fbase), rtc(time), ota(ota), wifi(wifi_manager), btn(button), dataClass(data_class)
                    {
                        day[0] = 0;
                        day[1] = 0;
                        night[0] = 0;
                        night[1] = 0;
                    }

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
    
    topScreen("Painel Principal", RIGHT);

    display->setTextDatum(TL_DATUM);
    drawArc(60 ,98, dataClass->getTemp(), dataClass->getTargetTemp(), 50, "Temperatura" , ".C");
	drawArc(160 ,98, dataClass->getHumid(), dataClass->getTargetHumid(), 100, "Umidade" , "%");
	drawArc(260 ,98, dataClass->getSoil(), dataClass->getTargetSoil(), 100, "Solo" , "%");
    
    actuatorDisplay();
    bottomScreen(" ", ">", "Iniciar", " ");

}
void Display::adjustScreen()
{
    topScreen("Ajustes", BOTH);
    bottomScreen("<", ">", "Ajustar", "Voltar");
}
void Display::confScreen()
{
    topScreen("Configuracao", LEFT);
    bottomScreen("<", " ", "Configurar", "Voltar");
}

void Display::menuSwitch(float *menu)
{
    switch (int(*menu))
    {
        case 0:
        {
            mainScreen();
            // if (btn[1]->read(menu, INCREMENT, 10, 0))
            //     flushScreen();
            break;
        }

        case 1:
        {
            adjustScreen();
            // if(btn[0]->read(menu, DECREMENT, 10, 0))
            //     flushScreen();
            // if(btn[1]->read(menu, INCREMENT, 10, 0))
            //     flushScreen();
            // if(btn[3]->read(menu, DECREMENT, 10, 0))
            //     flushScreen();
            break;
        }
        
        case 2:
        {
            confScreen();
            // if(btn[0]->read(menu, DECREMENT, 10, 0))
            //     flushScreen();
            
            // if(btn[3]->read(menu, DECREMENT, 10, 0))
            //     flushScreen();
            break;
        }




        default:
            break;
    }
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

void Display::topScreen(String label, int arrowSetup)
{
    const int height = display->fontHeight(2);
    const int textPos = (50 - height) / 2;
    const int sectionWidth = 440;
    const int textW = display->textWidth(label, 4);
    
    display->setTextDatum(MC_DATUM);
    display->setTextColor(WHITE, BLACK);

    int xStart = sectionWidth / 2;
    int leftTextX  = xStart - textW / 2;
    int rightTextX = xStart + textW / 2;

    display->drawString(label, xStart, textPos, 4);

    // ----- LEFT -----
    if (arrowSetup == LEFT || arrowSetup == BOTH)
    {
        int arrowX = leftTextX - 10 - 8;   // 8 = tamanho que você usa em animateArrow
        animateArrow(arrowX, textPos, 6, WHITE, LEFT);
    }

    // ----- RIGHT -----
    if (arrowSetup == RIGHT || arrowSetup == BOTH)
    {
        int arrowX = rightTextX + 10;
        animateArrow(arrowX, textPos, 6, WHITE, RIGHT);
    }

    // ----- TIME -----
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

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) 
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t getGradientColor(float angle) 
{
    angle = constrain(angle, 0, 270);
    float t;
    uint8_t r, g, b;

    if(angle <= 135) 
	{
        // Azul -> Verde
        t = angle / 135.0; // 0..1
        r = 0;
        g = t * 255;       // 0 -> 255
        b = 255 - t * 255; // 255 -> 0
    } else 
	{
        // Verde -> Vermelho
        t = (angle - 135) / 135.0; // 0..1
        r = t * 255;       // 0 -> 255
        g = 255 - t * 255; // 255 -> 0
        b = 0;
    }

    return color565(r, g, b);
}


void Display::drawArc(int x, int y, float value, float targetValue, float ceiling_value, String label, String unit)
{
	float angle = map(value, 0, ceiling_value, 0, 270);
	float step = 2;
	int radius = 43;

	String fValue = value < 10 ? "0" + String(value, 1) : String(value, 1);
	String fTarget = targetValue < 10 ? "0" + String(targetValue, 1) : String(targetValue, 1);

	display->setTextColor(WHITE, BLACK);
	display->setTextDatum(MC_DATUM);
	display->drawString(fValue, x , y, 4);
	display->drawString(fTarget, x , y + 15, 2);
	display->drawString(unit, x + radius - 20, y + radius - 10, 4);
	display->drawString(label, x , y + radius + 10, 2);



	for (float a = 0; a <= angle;  a += step)
	{
		uint16_t color = getGradientColor(a);
		display->drawArc(x, y, radius, 33 , a , a + step, color, BLACK);
	}
	
	display->drawArc(x, y, radius, 33 , angle + 1 , 270, DARK_GREY, BLACK);
}

String fmtNumber(int n)
{
  if (n < 10)
    return "0" + String(n);
  return String(n);
}

void Display::actuatorDisplay()
{
    display->setTextColor(WHITE, BLACK);
    dataClass->getDayTime(day);
    dataClass->getNightTime(night);

	// --- Monta período de luz ---
	int dayH = (int)day[0];
	int dayM = (int)day[1];
	int nightH = (int)night[0];
	int nightM = (int)night[1];

// Função auxiliar para colocar zero à esquerda
	display->setTextDatum(TL_DATUM);

	String lightPeriod = fmtNumber(dayH) + ":" + fmtNumber(dayM) +
                     " <-> " +
                     fmtNumber(nightH) + ":" + fmtNumber(nightM);

	// --- Labels ---
	const char* labels[] = {
	"Luz",
	"Rega",
	"Ventilador",
	"Aquecedor",
	"Umidificador",
	"Desumidificador"
	};

	// --- Status correspondentes ---
	bool statuses[6];
	statuses[0] = dataClass->getLightStatus();
	statuses[1] = dataClass->getPumpStatus();
	statuses[2] = dataClass->getCoolerStatus();
	statuses[3] = dataClass->getHeaterStatus();
	statuses[4] = dataClass->getHumidStatus();
	statuses[5] = dataClass->getDehumidStatus();

	// --- Posições e layout ---
	int baseXLabel = 325;
	int baseXValue = 450;
	int baseY = 86;
	int lineSpacing = 30;

	// --- Centraliza o período de luz ---
	int lightPeriodW = display->textWidth(lightPeriod, 2);
	display->setTextColor(WHITE, BLACK);
	display->drawString(lightPeriod, 300 + (lightPeriodW / 2), 56, 2);

	// --- Desenha todos os labels e status ---
	for (int i = 0; i < 6; i++) 
	{
		int y = baseY + i * lineSpacing;
		display->setTextColor(WHITE, BLACK);
		display->drawString(labels[i], baseXLabel, y, 2);
		display->drawString(formatStatus(statuses[i]), baseXValue, y, 2);
	}
	
	
}

String Display::formatStatus(bool status)
{
	String temp = "";
	if(!status)
	{
		display->setTextColor(RED, BLACK);
		temp = "OFF";
	}
	else
	{
		display->setTextColor(GREEN, BLACK);
		temp = "ON  ";
	}
	return temp;
}

