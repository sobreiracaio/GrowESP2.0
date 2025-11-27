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
    String foot = "       Aguarde       ";
    
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
void Display::mainScreen(float *menu)
{
    static float is_running = 0;
    if (*menu != 0)
        flushScreen();

    topScreen("Painel Principal", RIGHT);

    display->setTextDatum(TL_DATUM);
    drawArc(60 ,98, dataClass->getTemp(), dataClass->getTargetTemp(), 50, dataClass->getTempTolerance(), "Temperatura" , ".C", 1);
	drawArc(160 ,98, dataClass->getHumid(), dataClass->getTargetHumid(), 100, dataClass->getHumidTolerance(), "Umidade" , "%", 2);
	drawArc(260 ,98, dataClass->getSoil(), dataClass->getTargetSoil(), 100, dataClass->getSoilTolerance(), "Solo" , "%");
    drawBar(20, 170, 180, 15, dataClass->getWaterRes(), dataClass->getReservWarning(), 100, 2, "Reserv. Rega", "%");
    drawBar(20, 218, 180, 15, dataClass->getHumidRes(), dataClass->getReservWarning(), 100, 2,  "Reserv. Umid.", "%");
    
    actuatorDisplay();

    if (btn[1]->read(menu, INCREMENT, 10, 0))
                flushScreen();
                
                
    if(dataClass->getIsRunning() == false)
    {
        btn[2]->read(&is_running, CHANGE_STATE, 1, 0);
        dataClass->setIsRunning(is_running);

        bottomScreen(" ", ">", "Iniciar", "             ");
    }
    else
    {
        btn[3]->read(&is_running, CHANGE_STATE, 1, 0);
        dataClass->setIsRunning(is_running);

        bottomScreen(" ", ">", "                ", "Parar");
    }
}
void Display::adjustScreen(float *menu)
{
    static float submenu = 0;
    float value = 0;
    
    switch ((int)submenu)   
    {
    case 0:
        topScreen("Ajustes", BOTH);
        btn[0]->read(menu, DECREMENT, 10, 0);
        btn[1]->read(menu, INCREMENT, 10, 0);
        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(menu, DECREMENT, 10, 0);
        bottomScreen("<", ">", "Ajustar", "Voltar");
        break;

    case 1:
    {
        topScreen("Ajustes", BOTH, BLACK);
        btn[0]->read(&day[0], DECREMENT, 23, 0);
        btn[1]->read(&day[0], INCREMENT, 23, 0);
        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        dataClass->setDayTime(day[0], day[1]);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    
    case 2:
    {
        topScreen("Ajustes", BOTH, BLACK);
        btn[0]->read(&day[1], DECREMENT, 59, 0);
        btn[1]->read(&day[1], INCREMENT, 59, 0);
        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        dataClass->setDayTime(day[0], day[1]);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
        
    case 3:
        topScreen("Ajustes", BOTH, BLACK);
        btn[0]->read(&night[0], DECREMENT, 23, 0);
        btn[1]->read(&night[0], INCREMENT, 23, 0);
        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        dataClass->setNightTime(night[0], night[1]);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    
    case 4:
        topScreen("Ajustes", BOTH, BLACK);
        btn[0]->read(&night[1], DECREMENT, 59, 0);
        btn[1]->read(&night[1], INCREMENT, 59, 0);
        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        dataClass->setNightTime(night[0], night[1]);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
        
    case 5:
    {
        topScreen("Ajustes", BOTH, BLACK);
        
        value = dataClass->getTargetTemp();
        btn[0]->read(&value, DECREMENT, 50, 0, 0, 0.5);
        btn[1]->read(&value, INCREMENT, 50, 0, 0, 0.5);
        
        dataClass->setTargetTemp(value);
        
        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    
    case 6:
    {
        topScreen("Ajustes", BOTH, BLACK);
        value = dataClass->getTempTolerance();
        btn[0]->read(&value, DECREMENT, 10, 0, 0, 0.5);
        btn[1]->read(&value, INCREMENT, 10, 0, 0, 0.5);

        dataClass->setTempTolerance(value);

        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    
    case 7:
    {
        topScreen("Ajustes", BOTH, BLACK);
        value = dataClass->getTargetHumid();
        btn[0]->read(&value, DECREMENT, 100, 0, 0, 0.5);
        btn[1]->read(&value, INCREMENT, 100, 0, 0, 0.5);

        dataClass->setTargetHumid(value);


        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    case 8:
    {
        topScreen("Ajustes", BOTH, BLACK);
        value = dataClass->getHumidTolerance();
        btn[0]->read(&value, DECREMENT, 10, 0, 0, 0.5);
        btn[1]->read(&value, INCREMENT, 10, 0, 0, 0.5);

        dataClass->setHumidTolerance(value);

        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    case 9:
    {
        topScreen("Ajustes", BOTH, BLACK);
        value = dataClass->getTargetSoil();
        btn[0]->read(&value, DECREMENT, 100, 0, 0, 0.5);
        btn[1]->read(&value, INCREMENT, 100, 0, 0, 0.5);

        dataClass->setTargetSoil(value);

        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    case 10:
    {
        topScreen("Ajustes", BOTH, BLACK);
        value = dataClass->getPumpDuration();
        btn[0]->read(&value, DECREMENT, 120, 0, 0, 5);
        btn[1]->read(&value, INCREMENT, 120, 0, 0, 5);

        dataClass->setPumpDuration(value);

        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    case 11:
    {
        topScreen("Ajustes", BOTH, BLACK);
        value = dataClass->getAbsorptionDelay();
        btn[0]->read(&value, DECREMENT, 240, 0, 0, 5);
        btn[1]->read(&value, INCREMENT, 240, 0, 0, 5);

        dataClass->setAbsorptionDelay(value);

        btn[2]->read(&submenu, INCREMENT, 12, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    case 12:
    {
        topScreen("Ajustes", BOTH, BLACK);
        value = dataClass->getSoilTolerance();
        btn[0]->read(&value, DECREMENT, 20, 0);
        btn[1]->read(&value, INCREMENT, 20, 0);

        dataClass->setSoilTolerance(value);

        btn[2]->read(menu, DECREMENT, 10, 0);
        btn[3]->read(&submenu, DECREMENT, 12, 0);
        bottomScreen("-", "+", "    OK    ", "Voltar");
        break;
    }
    default:
        break;
    }
    
    int textH = display->fontHeight(2) + 6;
    
    display->drawSmoothRoundRect(10, 47, 5, 3, 459, 218, DARK_GREY, DARK_GREY);
    display->setTextColor(WHITE, BLACK);

    // Mapeamento: qual seção está ativa e qual campo está destacado
    // submenu: 0 = desabilitado, 1-4 = Fotoperiodo, 5-6 = Temperatura, 7-8 = Umidade, 9-12 = Rega
    struct Section {
        const char* label;
        int x;
        bool active;
    };
    
    Section sections[4] = {
        {"Fotoperiodo", 19, (submenu >= 1 && submenu <= 4)},
        {"Temperatura", 129, (submenu >= 5 && submenu <= 6)},
        {"Umidade", 239, (submenu >= 7 && submenu <= 8)},
        {"Rega", 349, (submenu >= 9 && submenu <= 12)}
    };

    // Desenha Fotoperiodo
    labelText(sections[0].x, 69, 109, 188, sections[0].label, 
              sections[0].active ? BLUE : DARK_GREY);
    inText(sections[0].x, 74, "Dia");
    boxText(36, 109, 32, 32,
        ((int)day[0] < 10 ? "0" + String((int)day[0]) : String((int)day[0])) + "h",
        submenu == 1 ? YELLOW : DARK_GREY, submenu == 1 ? true : false);
    display->drawString(":", 74, 118, 2);
    boxText(80, 109, 32, 32,
        ((int)day[1] < 10 ? "0" + String((int)day[1]) : String((int)day[1])) + "m",
        submenu == 2 ? YELLOW : DARK_GREY, submenu == 2 ? true : false);

    inText(sections[0].x, 160, "Noite");
    boxText(36, 196, 32, 32,
        ((int)night[0] < 10 ? "0" + String((int)night[0]) : String((int)night[0])) + "h",
        submenu == 3 ? YELLOW : DARK_GREY, submenu == 3 ? true : false);
    display->drawString(":", 74, 204, 2);
    boxText(80, 196, 32, 32,
        ((int)night[1] < 10 ? "0" + String((int)night[1]) : String((int)night[1])) + "m",
        submenu == 4 ? YELLOW : DARK_GREY, submenu == 4 ? true : false);

    // Desenha Temperatura
    labelText(sections[1].x, 69, 109, 188, sections[1].label,
              sections[1].active ? BLUE : DARK_GREY);
    inText(sections[1].x, 74, "Temp. Alvo");
    boxText(152, 109, 64, 32,
        (dataClass->getTargetTemp() < 10 ? "0" + String(dataClass->getTargetTemp()) : String(dataClass->getTargetTemp())) + " .C",
        submenu == 5 ? YELLOW : DARK_GREY, submenu == 5 ? true : false);

    inText(sections[1].x, 160, "Tolerancia");
    boxText(152, 196, 64, 32,
        (dataClass->getTempTolerance() < 10 ? "0" + String(dataClass->getTempTolerance()) : String(dataClass->getTempTolerance())) + " .C",
        submenu == 6 ? YELLOW : DARK_GREY, submenu == 6 ? true : false);

    // Desenha Umidade
    labelText(sections[2].x, 69, 109, 188, sections[2].label,
              sections[2].active ? BLUE : DARK_GREY);
    inText(sections[2].x, 74, "Umid. Alvo");
    boxText(262, 109, 64, 32,
        (dataClass->getTargetHumid() < 10 ? "0" + String(dataClass->getTargetHumid()) : String(dataClass->getTargetHumid())) + " %",
        submenu == 7 ? YELLOW : DARK_GREY, submenu == 7 ? true : false);

    inText(sections[2].x, 160, "Tolerancia");
    boxText(262, 196, 64, 32,
        (dataClass->getHumidTolerance() < 10 ? "0" + String(dataClass->getHumidTolerance()) : String(dataClass->getHumidTolerance())) + " %",
        submenu == 8 ? YELLOW : DARK_GREY, submenu == 8 ? true : false);

    // Desenha Rega
    labelText(sections[3].x, 69, 109, 188, sections[3].label,
              sections[3].active ? BLUE : DARK_GREY);
    inText(sections[3].x, 74, "Solo Alvo");
    boxText(372, 92, 64, textH,
        ((int)dataClass->getTargetSoil() < 10 ? "0" + String((int)dataClass->getTargetSoil()) : String((int)dataClass->getTargetSoil())) + " %",
        submenu == 9 ? YELLOW : DARK_GREY, submenu == 9 ? true : false);

    inText(sections[3].x, 116, "Duracao");
    boxText(372, 136, 64, textH,
        ((int)dataClass->getPumpDuration() < 10 ? "0" + String((int)dataClass->getPumpDuration()) : String((int)dataClass->getPumpDuration())) + " s",
        submenu == 10 ? YELLOW : DARK_GREY, submenu == 10 ? true : false);

    inText(sections[3].x, 160, "Intervalo");
    boxText(372, 180, 64, textH,
        ((int)dataClass->getAbsorptionDelay() < 10 ? "0" + String((int)dataClass->getAbsorptionDelay()) : String((int)dataClass->getAbsorptionDelay())) + " s",
        submenu == 11 ? YELLOW : DARK_GREY, submenu == 11 ? true : false);

    inText(sections[3].x, 204, "Tolerancia");
    boxText(372, 224, 64, textH,
        ((int)dataClass->getSoilTolerance() < 10 ? "0" + String((int)dataClass->getSoilTolerance()) : String((int)dataClass->getSoilTolerance())) + " %",
        submenu == 12 ? YELLOW : DARK_GREY, submenu == 12 ? true : false);

}
void Display::confScreen(float *menu)
{
    static float submenu = 0;
    float dummy_value = 0;
    static float lastSubmenu = -1;
    

    

    
        

    switch (int(submenu))
    {
        case 0:
            if(lastSubmenu != submenu)
            {
                display->fillRect(0, 70, 480, 200, BLACK);
                lastSubmenu = submenu;
            }
            topScreen("Configuracao", LEFT);
            drawMainTabs(0);
            drawWifiMenu(false);

            btn[0]->read(menu, DECREMENT, 10, 0);
            btn[1]->read(&submenu, INCREMENT, 8, 0, 0, 2);
            btn[2]->read(&submenu, INCREMENT, 8, 0);
            btn[3]->read(menu, DECREMENT, 10, 0);

            bottomScreen("<", ">", "  Config.  ", "Voltar");
            break;
        
        case 1:
        {
            
            topScreen("Configuracao", BOTH, BLACK);
            drawMainTabs(0);
            drawWifiMenu(true);

            if (btn[2]->read(&dummy_value, -1, 1, 0))
                wifi->handleReset();    
            btn[3]->read(&submenu, DECREMENT, 8, 0);
            bottomScreen("    ", "    ", "       OK       ", "Voltar");
            break;
        }

        case 2:
        {   
            if(lastSubmenu != submenu)
            {
                display->fillRect(0, 70, 480, 200, BLACK);
                lastSubmenu = submenu;
            }

            topScreen("Configuracao", BOTH, WHITE);
            drawMainTabs(1);
            drawUpdateMenu(-1, -1);

            btn[0]->read(&submenu, DECREMENT, 8, 0, 0, 2);
            btn[1]->read(&submenu, INCREMENT, 8, 0, 0, 6);
            btn[2]->read(&submenu, INCREMENT, 8, 0);
            btn[3]->read(&submenu, DECREMENT, 8, 0, 0, 2);

            bottomScreen("  <  ", "  >  ", "  Config.  ", "Voltar");
            break;
        }

        case 3:
        {   
            if(lastSubmenu != submenu)
            {
                display->fillRect(0, 70, 480, 200, BLACK);
                lastSubmenu = submenu;
            }

            topScreen("Configuracao", BOTH, BLACK);
            drawMainTabs(1);
            drawUpdateMenu(0, -1);

            btn[0]->read(&submenu, DECREMENT, 8, 0);
            btn[1]->read(&submenu, INCREMENT, 8, 0);
            if(btn[2]->read(&dummy_value, -1, 1, 0))
            {} // reinicia no modo atualizacao do sistema
            btn[3]->read(&submenu, DECREMENT, 8, 0);

            bottomScreen("  <  ", "  >  ", "Atualizar", "Voltar");
            break;
        }

        case 4:

            if(lastSubmenu != submenu)
            {
                display->fillRect(0, 70, 480, 200, BLACK);
                lastSubmenu = submenu;
            }

            topScreen("Configuracao", BOTH, BLACK);
            drawMainTabs(1);
            drawUpdateMenu(1, -1);

            
            btn[2]->read(&submenu, INCREMENT, 8, 0);
            btn[3]->read(&submenu, DECREMENT, 8, 0);

            bottomScreen("     ", "     ", "       OK       ", "Voltar");
            break;
        
        case 5:
        {
            if(lastSubmenu != submenu)
            {
                display->fillRect(0, 70, 480, 200, BLACK);
                lastSubmenu = submenu;
            }
            static float digit1 = 0;
            topScreen("Configuracao", BOTH, BLACK);
            drawMainTabs(1);
            drawUpdateMenu(1, -1);
            drawVersionInput(0);
            btn[0]->read(&digit1, DECREMENT, 9, 0);
            btn[1]->read(&digit1, INCREMENT, 9, 0);
            
            ota->setDigit(1,digit1);
           
            btn[2]->read(&submenu, INCREMENT, 8, 0);
            btn[3]->read(&submenu, DECREMENT, 8, 0);

            bottomScreen("  -  ", "  +  ", "   Prox.   ", "Voltar");
            break;
        }
        case 6:
        {
            static float digit2 = 0;
            topScreen("Configuracao", BOTH, BLACK);
            drawMainTabs(1);
            drawUpdateMenu(1, -1);
            drawVersionInput(1);
            btn[0]->read(&digit2, DECREMENT, 9, 0);
            btn[1]->read(&digit2, INCREMENT, 9, 0);
            
            ota->setDigit(2,digit2);

            btn[2]->read(&submenu, INCREMENT, 8, 0);
            btn[3]->read(&submenu, DECREMENT, 8, 0);

            bottomScreen("  -  ", "  +  ", "   Prox.   ", "Voltar");
            break;
        }
        case 7:
        {
            if(lastSubmenu != submenu)
            {
                display->fillRect(0, 70, 480, 200, BLACK);
                lastSubmenu = submenu;
            }
            static float digit3 = 0;
            topScreen("Configuracao", BOTH, BLACK);
            drawMainTabs(1);
            drawUpdateMenu(1, -1);
            drawVersionInput(2);
            btn[0]->read(&digit3, DECREMENT, 9, 0);
            btn[1]->read(&digit3, INCREMENT, 9, 0);
            
            ota->setDigit(3,digit3);

            if(btn[2]->read(&dummy_value, -1, 1, 0))
            {} // reinicia no modo atualizacao de modulo
            btn[3]->read(&submenu, DECREMENT, 8, 0);

            bottomScreen("  -  ", "  +  ", "Atualizar", "Voltar");
            break;
        }
        case 8:
            if(lastSubmenu != submenu)
            {
                display->fillRect(0, 70, 480, 200, BLACK);
                lastSubmenu = submenu;
            }

            topScreen("Configuracao", LEFT);
            drawMainTabs(2);
            drawAboutMenu();

            btn[0]->read(&submenu, DECREMENT, 8, 0, 0, 6);

            if(btn[3]->read(menu, DECREMENT, 10, 0, 0))
                submenu = 0;

            bottomScreen(" < ", "   ", "                 ", "Voltar");
            break;
        
        
        default:
            break;
        }
}


void Display::menuSwitch(float *menu)
{
    static float lastmenu = -1;
    if(lastmenu != *menu)
    {
        flushScreen();
        lastmenu = *menu;
    }

    switch (int(*menu))
    {
        case 0:
        {
            mainScreen(menu);
            
            break;
        }

        case 1:
        {
            adjustScreen(menu);
            break;
        }
        
        case 2:
        {
            confScreen(menu);
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

void Display::topScreen(String label, int arrowSetup, int color)
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
    if (arrowSetup == LEFT)
    {
        int arrowL = leftTextX - 10 - 8;   // 8 = tamanho que você usa em animateArrow
        int arrowR = rightTextX + 10;
        animateArrow(arrowL, textPos, 6, color, LEFT);
        animateArrow(arrowR, textPos, 6, BLACK, RIGHT);
    }

    // ----- RIGHT -----
    if (arrowSetup == RIGHT)
    {
        int arrowL = leftTextX - 10 - 8;   // 8 = tamanho que você usa em animateArrow
        int arrowR = rightTextX + 10;
        animateArrow(arrowL, textPos, 6, BLACK, LEFT);
        animateArrow(arrowR, textPos, 6, color, RIGHT);
    }

    if(arrowSetup == BOTH)
    {
        int arrowL = leftTextX - 10 - 8;   // 8 = tamanho que você usa em animateArrow
        int arrowR = rightTextX + 10;
        animateArrow(arrowL, textPos, 6, color, LEFT);
        animateArrow(arrowR, textPos, 6, color, RIGHT);
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

    // Converte cor BLACK para RGB
    uint8_t blackR = ((BLACK >> 11) & 0x1F) * 255 / 31;
    uint8_t blackG = ((BLACK >> 5) & 0x3F) * 255 / 63;
    uint8_t blackB = (BLACK & 0x1F) * 255 / 31;

    // Converte cor original para RGB
    uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
    uint8_t b = (color & 0x1F) * 255 / 31;

    // Interpola entre BLACK e a cor desejada baseado no brightness
    r = blackR + ((r - blackR) * brightness) / 255;
    g = blackG + ((g - blackG) * brightness) / 255;
    b = blackB + ((b - blackB) * brightness) / 255;

    uint16_t fadeColor = display->color565(r, g, b);

    // Desenha triângulo apontando para a esquerda
    if (orientation == LEFT)
    {
        display->fillTriangle(
            x,       y,          // ponto esquerdo
            x+size,  y-size,     // topo
            x+size,  y+size,     // base
            fadeColor
        );
    }
    
    // Desenha triângulo apontando para a direita
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

// Função auxiliar para interpolar entre duas cores
uint16_t interpolateColor(uint16_t color1, uint16_t color2, float t) 
{
    // Extrai RGB de color1 (RGB565)
    uint8_t r1 = ((color1 >> 11) & 0x1F) * 255 / 31;
    uint8_t g1 = ((color1 >> 5) & 0x3F) * 255 / 63;
    uint8_t b1 = (color1 & 0x1F) * 255 / 31;
    
    // Extrai RGB de color2 (RGB565)
    uint8_t r2 = ((color2 >> 11) & 0x1F) * 255 / 31;
    uint8_t g2 = ((color2 >> 5) & 0x3F) * 255 / 63;
    uint8_t b2 = (color2 & 0x1F) * 255 / 31;
    
    // Interpola cada canal
    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t b = b1 + (b2 - b1) * t;
    
    return color565(r, g, b);
}

// Gradiente com 3 cores
uint16_t getGradientColor(float angle, uint16_t c1 = RED, uint16_t c2 = YELLOW, uint16_t c3 = GREEN) 
{
    angle = constrain(angle, 0, 270);
    
    if (angle <= 135) 
    {
        // Primeiro terço: c1 -> c2
        float t = angle / 135.0; // 0..1
        return interpolateColor(c1, c2, t);
    } 
    else 
    {
        // Segundo terço: c2 -> c3
        float t = (angle - 135) / 135.0; // 0..1
        return interpolateColor(c2, c3, t);
    }
}


void Display::drawArc(int x, int y, float value, float targetValue, float ceiling_value, float tolerance, String label, String unit, int option)
{
	float angle = map(value, 0, ceiling_value, 0, 270);
    float targetAngle1  = map(targetValue - tolerance, 0, ceiling_value, 0, 270);
    float targetAngle2  = map(targetValue + tolerance, 0, ceiling_value, 0, 270);
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
        uint16_t color = 0;
        if (option == 1)
            color = getGradientColor(a, BLUE, YELLOW, RED);
        else if (option == 2)
            color = getGradientColor(a, YELLOW, BLUE, PURPLE);
        else
		    color = getGradientColor(a);
		display->drawArc(x, y, radius, 33 , a - 1 , a + step, color, BLACK);
	}
	
	display->drawArc(x, y, radius, 33 , angle + 1 , 270, DARK_GREY, BLACK);
    display->drawArc(x, y, radius + 5, 45, targetAngle1, targetAngle2, YELLOW, BLACK);
}

void Display::drawBar(int x, int y, int width, int height, float value, float targetValue, float ceiling_value, float tolerance, String label, String unit)
{
    // Mapeia o valor atual para a largura da barra
    float measure = map(value, 0, ceiling_value, 0, width);
    float targetPos = map(targetValue, 0, ceiling_value, 0, width);
    float toleranceWidth = map(tolerance, 0, ceiling_value, 0, width);
    
    float step = 2;
    
    // Textos
    display->setTextColor(WHITE, BLACK);
    display->setTextDatum(TL_DATUM);
    
    String fValue = value < 10 ? "0" + String(value, 1) : String(value, 1);
    String fTarget = targetValue < 10 ? "0" + String(targetValue, 1) : String(targetValue, 1);
    
    // Label abaixo da barra
    display->drawString(label, x, y + height + 5, 2);
    
    // Valores (ajuste posição conforme necessário)
    // display->drawString(fValue, width + 5, y + height + 5, 2);
    
    display->drawString(fValue + unit, width - 20, y + height + 5 , 2);
    
    // Desenha a barra com gradiente
    for (float a = 0; a <= measure; a += step)
    {
        // Mapeia a posição atual para um ângulo (0-270) para usar o gradiente existente
        float angle = map(a, 0, width, 0, 270);
        uint16_t color = getGradientColor(angle);
        
        display->fillRect(x + a - 1, y, step, height, color);
    }
    
    // Parte não preenchida (cinza escuro)
    if (measure < width) {
        display->fillRect(x + measure, y, width - measure - 2, height, DARK_GREY);
    }
    
    // Desenha a faixa de tolerância (amarela) - como uma linha ou retângulo fino
    float targetStart = targetPos - toleranceWidth;
    float targetEnd = targetPos + toleranceWidth;
    
    // Garante que não sai dos limites
    if (targetStart < 0) targetStart = 0;
    if (targetEnd > width) targetEnd = width;
    
    // Desenha indicador de target (pode ser uma linha ou retângulo acima/abaixo da barra)
    display->fillRect(x + targetStart, y - 3, targetEnd - targetStart, 2, YELLOW);
    
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

void Display::labelText(int x, int y, int rectW, int rectH, String label, int color)
{
	int textWidth = display->textWidth(label, 2);
    int textX = x + (rectW - textWidth) / 2;

    display->drawSmoothRoundRect(x, y, 5, 3, rectW, rectH, color, color);
    display->drawString(label, textX, 50, 2);
}

void Display::boxText(int x, int y, int rectW, int rectH, String label, int color, bool animate)
{
    int font = 2;
    int textWidth = display->textWidth(label, font);
    int textHeight = display->fontHeight(font);

    // Centraliza texto no retângulo
    int textX = x + (rectW - textWidth) / 2;
    int textY = y + (rectH - textHeight) / 2;

    uint16_t finalColor = color;

    if (animate) 
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

        // Converte DARK_GREY (cor mínima) de RGB565 para RGB
        uint8_t r_dark = ((DARK_GREY >> 11) & 0x1F) * 255 / 31;
        uint8_t g_dark = ((DARK_GREY >> 5) & 0x3F) * 255 / 63;
        uint8_t b_dark = (DARK_GREY & 0x1F) * 255 / 31;

        // Converte color (cor máxima) de RGB565 para RGB
        uint8_t r_bright = ((color >> 11) & 0x1F) * 255 / 31;
        uint8_t g_bright = ((color >> 5) & 0x3F) * 255 / 63;
        uint8_t b_bright = (color & 0x1F) * 255 / 31;

        // Interpola entre DARK_GREY e color baseado no brightness (0..255)
        float t = brightness / 255.0; // Normaliza para 0..1
        uint8_t r = r_dark + (r_bright - r_dark) * t;
        uint8_t g = g_dark + (g_bright - g_dark) * t;
        uint8_t b = b_dark + (b_bright - b_dark) * t;

        finalColor = color565(r, g, b);
    }

    // Desenha o retângulo e o texto centralizado
    display->setTextDatum(TL_DATUM);
    display->drawSmoothRoundRect(x, y, 5, 3, rectW, rectH, finalColor, finalColor);
    display->setTextColor(WHITE, BLACK);
    display->drawString(label, textX, textY, font);
}

void Display::inText(int x, int y, String label)
{
	int rectWidth = 109; 
	int textWidth = display->textWidth(label, 2);
	int textX = x + (rectWidth - textWidth) / 2;
	display->drawString(label, textX, y, 2);


}


void Display::drawMainTabs(int activeTab)
{
    display->setTextDatum(TL_DATUM);
    boxText(20, 60, 146, 40, "WiFi", activeTab == 0 ? BLUE : DARK_GREY);
    boxText(166, 60, 146, 40, "Atualizacao", activeTab == 1 ? BLUE : DARK_GREY);
    boxText(312, 60, 146, 40, "Sobre", activeTab == 2 ? BLUE : DARK_GREY);
}

void Display::drawWifiMenu(bool selected)
{
    boxText(160, 150, 160, 40, "Alterar Rede e Senha", selected ? YELLOW : DARK_GREY);
    display->setTextDatum(MC_DATUM);
    display->drawString("*O equipamento ira reinicializar!", 240, 212, 2);
    display->setTextDatum(TL_DATUM);
}

void Display::drawUpdateMenu(int selectedOption, int highlightedDigit)
{
    boxText(80, 150, 160, 40, "Atualizar Sistema", selectedOption == 0 ? YELLOW : DARK_GREY);
    boxText(240, 150, 160, 40, "Atualizar modulo", selectedOption == 1 ? YELLOW : DARK_GREY);
}

void Display::drawVersionInput(int highlightedDigit)
{
    String digit1 = String(ota->getDigit(1));
    String digit2 = String(ota->getDigit(2));
    String digit3 = String(ota->getDigit(3));
    if (highlightedDigit < 0) return; // Não desenha se não houver seleção
    
    display->setTextDatum(MC_DATUM);
    display->drawString("Digite a versao do seu modulo:", 240, 200, 2);
    display->setTextDatum(TL_DATUM);
    
    boxText(165, 215, 40, 40, digit1, highlightedDigit == 0 ? GREEN : DARK_GREY, highlightedDigit == 0 ? true : false);
    boxText(215, 215, 40, 40, digit2, highlightedDigit == 1 ? GREEN : DARK_GREY, highlightedDigit == 1 ? true : false);
    boxText(265, 215, 40, 40, digit3, highlightedDigit == 2 ? GREEN : DARK_GREY, highlightedDigit == 2 ? true : false);
}

void Display::drawAboutMenu()
{
    display->setTextDatum(MC_DATUM);
    display->drawString("Growbox v1.0", 240, 130, 4);
    display->drawString("Versao do modulo : 101", 240, 170, 2);
    display->drawString("Instagram: @pagina", 240, 195, 2);
    display->setTextDatum(TL_DATUM);
}