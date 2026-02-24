#include "Display.hpp"

Display::Display(TFT_eSPI *tft, FBase *fbase, Time *time, OTA *ota, WifiManager *wifi_manager, Button **button, DataClass *data_class) :
                    display(tft), firebase(fbase), rtc(time), ota(ota), wifi(wifi_manager), btn(button), dataClass(data_class) 
                    {
                        for (int i = 0; i < 2; i++)
                        {
                            day[i] = 0;
                            night[i] = 0;
                        }

                        memset(gradientCache, 0, sizeof(gradientCache));
                        gaugeSprite = new TFT_eSprite(display);
                        gaugeSprite->createSprite(110, 120);
                        gaugeSprite->setColorDepth(16); // 16-bit color
                        
                    }


bool Display::initDisplay()
{
    display->init();
    display->setRotation(1);
    display->setSwapBytes(true);

   
   

    ledcAttachPin(TFT_BL, 0);
    ledcSetup(0, 5000, 8);
    
    ledcWrite(0, 255);

    display->fillScreen(BLACK);
    return true;
}

void Display::injectFBase(FBase *fbase)
{
    firebase = fbase;
}



void Display::logoScreen(String text)
{
    
    display->fillScreen(BG_ICON);
    
    //display->pushImage(141, 35, 197, 248, logoIcon);
    
    display->setTextColor(WHITE, BG_ICON);
    display->setTextDatum(MC_DATUM);
    display->drawString(text, 240, 300, 2);
}

void Display::drawBackGround(int color)
{
    display->fillScreen(BLACK);
    
    display->fillRoundRect(5, 40, 470, 240, 10, color);
}


void Display::boxButton(int x, int y, int w, int h, int color, const unsigned char *image, bool is_selected, String text_top, String text_bot, int text_size)
{
    int pulsed_color = is_selected ? pulseColorTimed(color, DARK_GREY, text_size) : color;


    for (float i = 0; i < 4; i += 1)
        display->drawRoundRect(x + i, y + i, w - (i * 2), h - (i * 2), 10 - i, pulsed_color);

    display->setTextColor(color, DARK_GREY);
    display->drawString(text_top, x + (w - display->textWidth(text_top, text_size)) / 2, y + 10, text_size);
    display->drawString(text_bot, x + (w - display->textWidth(text_bot, text_size)) / 2, y + (h - display->fontHeight(text_size)) - 5, text_size);

    if (image != NULL)
        display->drawBitmap(x + (w - 64) / 2, y + 25, image, 64, 64, color);
}

void Display::buttonScreen(String labels[2][4], int colors[2][4], const unsigned char *images[2][4], int *menu, int menu_nbr)
{
    int distance_h = 113;
    int distance_v = 115;
    static int selection = 0;
    int is_selected[2][4] ={{0, 1, 2, 3},{4, 5, 6, 7}};

    if(selection < 0)
        selection = menu_nbr;
    if(selection > menu_nbr)
        selection = 0;

    
    if(btn[0]->read())
        selection--;
    if(btn[1]->read())
        selection++;
    if(btn[2]->read())
    {
       
        *menu = selection;
    }
    if(btn[3]->read())
    {
        if(selection != 0)
            selection = 0;
        else
            *menu = -1;

    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            boxButton(16 + (distance_h * i), 48 + (distance_v * j), 108, 108, colors[j][i], images[j][i], selection == is_selected[j][i] ? true : false, "", labels[j][i]);
            //display->drawRect(38 + (distance_h * i), 48 + (distance_v * j) + 22, 64, 64, colors[j][i]);
        }
    }
}

uint16_t Display::pulseColorTimed(uint16_t c1, uint16_t c2, float speed)
{
    static float t = 0.0f;
    static int dir = 1;
    static unsigned long last = 0;

    unsigned long now = millis();
    float dt = (now - last) / 1000.0f; // delta time em segundos
    last = now;

    t += dir * speed * dt;

    if (t >= 1.0f) { t = 1.0f; dir = -1; }
    if (t <= 0.0f) { t = 0.0f; dir =  1; }

    uint8_t r1 = (c1 >> 11) & 0x1F;
    uint8_t g1 = (c1 >> 5)  & 0x3F;
    uint8_t b1 =  c1        & 0x1F;

    uint8_t r2 = (c2 >> 11) & 0x1F;
    uint8_t g2 = (c2 >> 5)  & 0x3F;
    uint8_t b2 =  c2        & 0x1F;

    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t b = b1 + (b2 - b1) * t;

    return (r << 11) | (g << 5) | b;
}

void Display::drawAntenna()
{
    display->drawLine(55, 12, 55, 25, wifi->getStatus() ? GREEN : RED);
    display->drawLine(50, 12, 55, 17, wifi->getStatus() ? GREEN : RED);
    display->drawLine(60, 12, 55, 17, wifi->getStatus() ? GREEN : RED);

    display->drawLine(58, 22, 61, 25, wifi->getStatus() ? BLACK : RED);
    display->drawLine(61, 22, 58, 25, wifi->getStatus() ? BLACK : RED);
    
}

void Display::topScreen(String label)
{
    String hour = wifi->getStatus() && rtc->getHour() != -1 ? (rtc->getHour() < 10 ? "0" + String(rtc->getHour()) : String(rtc->getHour())) : "00";
    String min = wifi->getStatus() && rtc->getMinute() != -1 ? (rtc->getMinute() < 10 ? "0" + String(rtc->getMinute()) : String(rtc->getMinute())) : "00";
    display->setTextColor(WHITE, BLACK);
    display->setTextDatum(MC_DATUM);
    display->drawString(label, 240, 20, 4);
    display->setTextDatum(TL_DATUM);
    const int height = display->fontHeight(2);
    const int textPos = (40 - height) / 2;
    const int sectionWidth = 440;
    wifiConnStatus();
    drawAntenna();
    String time = wifi->getStatus() ? hour + ":" + min : "00:00";
    //String time = String(rtc->getHour()) + ":" + (rtc->getMinute() < 10 ? "0" + String(rtc->getMinute()) : String(rtc->getMinute()));
    const int timeX = 410; 
    display->drawString(time, timeX, textPos, 4);
}

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) 
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

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

void Display::drawArcGauge(int x, int y, float value, float targetValue, float ceiling_value, 
                      float tolerance, String label, String unit, int option)
    {
        if (!gradientCached) {
            for (int a = 0; a < 270; a++) {
                gradientCache[0][a] = getGradientColor(a);
                gradientCache[1][a] = getGradientColor(a, BLUE, YELLOW, RED);
                gradientCache[2][a] = getGradientColor(a, YELLOW, BLUE, PURPLE);
            }
            gradientCached = true;
        }
        
        float angle = constrain(map(value, 0, ceiling_value, 0, 270), 0, 270);
        float targetAngle1 = constrain(map(targetValue - tolerance, 0, ceiling_value, 0, 270), 1, 269);
        float targetAngle2 = constrain(map(targetValue + tolerance, 0, ceiling_value, 0, 270), 1, 269);
        
        int radius = 43;
        if (value > ceiling_value) value = ceiling_value;

        String fValue = value >= 100 ? String(int(value)) : String(value, 1);
        String fTarget = targetValue >= 100 ? String(int(targetValue)) : String(targetValue, 1);

        gaugeSprite->fillSprite(DARK_GREY);
        
        int cx = 55;
        int cy = 55;

        // Desenhar arco com segmentos de 15 graus (apenas 18 iterações!)
        int step = 15;
        int opt_idx = option == 1 ? 1 : (option == 2 ? 2 : 0);
        
        for (int a = 0; a <= (int)angle; a += step)
        {
            int endAngle = min(a + step, (int)angle);
            uint16_t color = gradientCache[opt_idx][a];
            gaugeSprite->drawArc(cx, cy, radius, 33, a, endAngle, color, DARK_GREY);
        }
        
        gaugeSprite->drawArc(cx, cy, radius, 33, angle, 270, DARK_GREY, DARK_GREY);
        gaugeSprite->drawArc(cx, cy, radius + 5, 45, targetAngle1, targetAngle2, YELLOW, DARK_GREY);

        gaugeSprite->setTextColor(WHITE, DARK_GREY);
        gaugeSprite->setTextDatum(MC_DATUM);
        gaugeSprite->drawString(fValue, cx, cy, 4);
        gaugeSprite->drawString(fTarget, cx, cy + 15, 2);
        gaugeSprite->drawString(unit, cx + radius - 20, cy + radius - 10, 2);
        gaugeSprite->drawString(label, cx, cy + radius + 15, 2);

        gaugeSprite->pushSprite(x - cx, y - cy);
    }


void Display::botScreen(String labels[4])
{
    int distance = 113;

    
    display->setTextColor(WHITE, BLACK);
    display->setTextDatum(MC_DATUM);

    display->fillCircle(68, 307, 8, WHITE);
    display->drawString(labels[0], 69, 290, 2);

    display->fillCircle(68 + distance, 307, 8, WHITE);
    display->drawString(labels[1], 69 + distance, 290, 2);

    display->fillCircle(68 + distance * 2, 307, 8, GREEN);
    display->drawString(labels[2], 69 + distance * 2, 290, 2);

    display->fillCircle(68 + distance * 3, 307, 8, RED);
    display->drawString(labels[3], 69 + distance * 3, 290, 2);

    display->setTextDatum(TL_DATUM);
}

void Display::mainScreen(int *menu)
{
    
    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen();
    botScreen(label);
    String labels[2][4] ={{"Monitorar", "Fotoperiodo", "Temperatura", "Umidade"},{"Rega", "Calibrar", "Configuracoes", ""}};
    int colors[2][4] = {{WHITE, YELLOW, RED, BLUE},{BROWN, ORANGE, PURPLE, DARK_GREY}};
    const unsigned char *images[2][4] ={{monitorIcon, lightPeriodIcon, tempIcon, humidIcon},{wateringIcon, calibIcon, confIcon, NULL}};
    
    
    buttonScreen(labels, colors, images, menu, 6);
}

void Display::monitorMenu(int *menu)
{
    bool is_running = dataClass->getIsRunning();
    static int selection = 0;

    if (selection < 0)
        selection = 0;
    if (selection > 3)
        selection = 3;

    String labels[4] = {" ", " ", is_running ? " Parar " : " Iniciar ", "Voltar"};
    topScreen("Monitoramento");
    botScreen(labels);

    if(btn[0]->read())
        ;
    if(btn[1]->read())
        ;
    if(btn[2]->read())
    {
       is_running = !is_running; 
       dataClass->setIsRunning(is_running);
       sendPacket(STATUS0, is_running);
       is_running = dataClass->getIsRunning();
       firebase->aSyncSetBool(safeEmail + "/Status", is_running);

       if(selection != 3)
        selection++;
        else
        {
            selection = 0;
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
        selection--;
        else
        {
            selection = 0;
            *menu = -1;
        }
    }

    int distance = 119;
    String gauge_labels[4] = {"Temperatura", "Umidade", "Solo", "Reserv."};
    String gauge_units[4] = {".C", "%", "%", "%"};
    float gauge_values[4] = {dataClass->getTemp(), dataClass->getHumid(), dataClass->getCalibratedSoil(), dataClass->getWaterCalibrated()};
    float gauge_Tvalues[4] = {dataClass->getTargetTemp(), dataClass->getTargetHumid(), dataClass->getTargetSoil() == 150 ? 0 : dataClass->getTargetSoil(), 0};
    float gauge_Cvalues[4] = {50, 100, 100, 100};
    float gauge_tolerances[4] = {dataClass->getTempTolerance(), dataClass->getHumidTolerance(), dataClass->getSoilTolerance(), 0};
    int gauge_options[4] = {1, 2, 0, 2};
    for (int i = 0; i < 4; i++)
    {
        drawArcGauge(60 + (distance * i), 95, gauge_values[i], gauge_Tvalues[i], gauge_Cvalues[i], gauge_tolerances[i], gauge_labels[i], gauge_units[i], gauge_options[i]);
    }
    display->setTextDatum(TL_DATUM);
    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Resumo dos perifericos", 120, 165, 4);
    
    int distance_y = 30;
    int distance_x = 230;
    String devices[2][3] = {{"Luz", "Bomba", "Cooler"}, {"Aquecedor", "Umid.", "Desumid."}};
    String status[2][3] = {{dataClass->getLightStatus() ? "ON  " : "OFF", dataClass->getPumpStatus() ? "ON  " : "OFF", dataClass->getCoolerStatus() ? "ON  " : "OFF"},
                           {dataClass->getHeaterStatus() ? "ON  " : "OFF", dataClass->getHumidStatus() ? "ON  " : "OFF", dataClass->getDehumidStatus() ? "ON  " : "OFF"}};
    String on_time[2][3] = {{"12:00", "12:01", "12:02"},{"12:03", "12:04", "12:05"}};
    for (int j = 0; j < 2; j++)
    {
        for(int i = 0; i < 3; i++)
        {
            display->drawString(devices[j][i] + " ->", 30 + distance_x * j, 195 + distance_y * i, 2);
            display->setTextColor(status[j][i] == "ON  " ? GREEN : RED, DARK_GREY);
            display->drawString(status[j][i], 120 + distance_x * j, 195 + distance_y * i, 2);
            display->setTextColor(WHITE, DARK_GREY);
            display->drawString("<-" + on_time[j][i], 164 + distance_x * j, 195 + distance_y * i, 2);
        }
    };



}

void Display::lightMenu(int *menu)
{
    static int selection = 0;

    if (selection < 0)
        selection = 0;
    if (selection > 3)
        selection = 3;
    
    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Fotoperiodo");
    botScreen(labels);

    

    if(btn[0]->read())
    {
        if(selection == 0)
        {
            day[0]--;
            if (day[0] > 23)
                day[0] = 0;
            if (day[0] < 0)
                day[0] = 23;
        }
        if(selection == 1)
        {
            day[1]--;
            if (day[1] > 59)
                day[1] = 0;
            if (day[1] < 0)
                day[1] = 59;
        }
        if(selection == 2)
        {
            night[0]--;
            if (night[0] > 23)
                night[0] = 0;
            if (night[0] < 0)
                night[0] = 23;
        }
        if(selection == 3)
        {
            night[1]--;
            if (night[1] > 59)
                night[1] = 0;
            if (night[1] < 0)
                night[1] = 59;
        }
    }

    if(btn[1]->read())
    {
        if(selection == 0)
        {
            day[0]++;
            if (day[0] > 23)
                day[0] = 0;
            if (day[0] < 0)
                day[0] = 23;
        }
        if(selection == 1)
        {
            day[1]++;
            if (day[1] > 59)
                day[1] = 0;
            if (day[1] < 0)
                day[1] = 59;
        }
        if(selection == 2)
        {
            night[0]++;
            if (night[0] > 23)
                night[0] = 0;
            if (night[0] < 0)
                night[0] = 23;
        }
        if(selection == 3)
        {
            night[1]++;
            if (night[1] > 59)
                night[1] = 0;
            if (night[1] < 0)
                night[1] = 59;
        }
    }

    if(btn[2]->read())
    {
        String hour = "";

        if(selection == 0)
        {
            dataClass->setDayTime(day[0], day[1]);
            hour = (day[0] < 10 ? "0" + String(day[0]) : String(day[0])) + ":" + (day[1] < 10 ? "0" + String(day[1]) : String(day[1]));
            firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOn", hour);
            
        }

        if(selection == 1)
        {
            dataClass->setDayTime(day[0], day[1]);
            hour = (day[0] < 10 ? "0" + String(day[0]) : String(day[0])) + ":" + (day[1] < 10 ? "0" + String(day[1]) : String(day[1]));
            firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOn", hour);
        }

        if(selection == 2)
        {
            dataClass->setNightTime(night[0], night[1]);
            hour = (night[0] < 10 ? "0" + String(night[0]) : String(night[0])) + ":" + (night[1] < 10 ? "0" + String(night[1]) : String(night[1]));
            firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOff", hour);
        }

        if(selection == 3)
        {
            dataClass->setNightTime(night[0], night[1]);
            hour = (night[0] < 10 ? "0" + String(night[0]) : String(night[0])) + ":" + (night[1] < 10 ? "0" + String(night[1]) : String(night[1]));
            firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOff", hour);
        }

        
        if(selection != 3)
            selection++;
        else
        {
            selection = 0;
            *menu = -1;        
        }
    }

    if(btn[3]->read())
    {
        String hour = "";

        if(selection == 0)
        {
            dataClass->setDayTime(day[0], day[1]);
            hour = (day[0] < 10 ? "0" + String(day[0]) : String(day[0])) + ":" + (day[1] < 10 ? "0" + String(day[1]) : String(day[1]));
            firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOn", hour);
            
        }

        if(selection == 1)
        {
            dataClass->setDayTime(day[0], day[1]);
            hour = (day[0] < 10 ? "0" + String(day[0]) : String(day[0])) + ":" + (day[1] < 10 ? "0" + String(day[1]) : String(day[1]));
            firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOn", hour);
        }

        if(selection == 2)
        {
            dataClass->setNightTime(night[0], night[1]);
            hour = (night[0] < 10 ? "0" + String(night[0]) : String(night[0])) + ":" + (night[1] < 10 ? "0" + String(night[1]) : String(night[1]));
            firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOff", hour);
        }

        if(selection == 3)
        {
            dataClass->setNightTime(night[0], night[1]);
            hour = (night[0] < 10 ? "0" + String(night[0]) : String(night[0])) + ":" + (night[1] < 10 ? "0" + String(night[1]) : String(night[1]));
            firebase->aSyncSetString(safeEmail + "/InsertedData/Light/HourOff", hour);
        }
            
        
        if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -1;
        }
    }

    
    
    int data[4] = {day[0], day[1], night[0], night[1]};
    String format_data[4] = {"","","",""};
    
    for (int i = 0; i < 4; i++)
    {
       data[i] < 10 ? format_data[i] = "0" + String(int(data[i])) : format_data[i] = String(int(data[i]));
        
    }
    
    
    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);

    display->drawString(format_data[0], 224, 85, 8);
    display->drawString(":", 296, 75, 8);
    display->drawString(format_data[1], 374, 85, 8);

    display->drawString(format_data[2], 224, 200, 8);
    display->drawString(":", 296, 190, 8);
    display->drawString(format_data[3], 374, 200, 8);


    display->setTextDatum(TL_DATUM);
    boxButton(50, 48, 108, 108, YELLOW, sunIcon, false, "Relogio", "DIA", 2);
    boxButton(170, 130, 108, 26, YELLOW, NULL, selection == 0 ? true : false, "", "- HORA +", 2);
    boxButton(320, 130, 108, 26, YELLOW, NULL, selection == 1 ? true : false, "", "- MIN +", 2);

    boxButton(50, 163, 108, 108, BLUE, moonIcon, false, "", "NOITE", 2);
    boxButton(170, 245, 108, 26, BLUE, NULL, selection == 2 ? true : false, "", "- HORA +", 2);
    boxButton(320, 245, 108, 26, BLUE, NULL, selection == 3 ? true : false, "", "- MIN +", 2);

    //Serial.printf("Value is %f, Day: %f : %f, Night: %f : %f\n", value, day[0], day[1], night[0], night[1]);
}

void Display::tempMenu(int *menu)
{
    static int selection = 0;
    static float temp_value = dataClass->getTargetTemp();
    static float tol_value = dataClass->getTempTolerance();


    if (selection < 0)
        selection = 0;
    if (selection > 1)
        selection = 1;

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Temperatura");
    botScreen(labels);

    if(btn[0]->read())
    {
        
        if(selection == 0)
        {
            temp_value -= 0.5;
            if (temp_value > 60)
                temp_value = 0;
            if (temp_value < 0)
                temp_value = 60;
        }

        if(selection == 1)
        {
            tol_value -= 0.5;
            if (tol_value > 10)
                tol_value = 1;
            if (tol_value < 1)
                tol_value = 10;
        }
    }
    if(btn[1]->read())
    {
        if(selection == 0)
        {
            temp_value += 0.5;
            if (temp_value > 60)
                temp_value = 0;
            if (temp_value < 0)
                temp_value = 60;
        }

        if(selection == 1)
        {
            tol_value += 0.5;
            if (tol_value > 10)
                tol_value = 1;
            if (tol_value < 1)
                tol_value = 10;
        }
    }
    if(btn[2]->read())
    {
        if(selection == 0)
        {
            dataClass->setTargetTemp(temp_value);
            sendPacket(TT, temp_value); 
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp", temp_value);
        }
        if(selection == 1)
        {
            dataClass->setTempTolerance(tol_value);
            sendPacket(TTOL, tol_value);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", tol_value);
        }

        

       if(selection != 1)
            selection++;
        else
        {
            selection = 0;
            *menu = -1;        
        }
    }
    if(btn[3]->read())
    {
        if(selection == 0)
        {
            dataClass->setTargetTemp(temp_value);
            sendPacket(TT, temp_value); 
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp", temp_value);
        }
        if(selection == 1)
        {
            dataClass->setTempTolerance(tol_value);
            sendPacket(TTOL, tol_value);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", tol_value);
        }

        //temp_value, tol_value = 0;

        if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -1;
        }
    }

    String format_data[2] = {temp_value < 10 ? " " + String(temp_value,1) + " " : String(temp_value,1), 
                                tol_value < 10 ? " " + String(tol_value,1) + " " : String(tol_value,1)};

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);

    display->drawString(format_data[0], 320, 85, 8);
    display->drawString(format_data[1], 320, 200, 8);

    display->setTextDatum(TL_DATUM);
    boxButton(50, 48, 108, 108, RED, tempIcon, false, "", "TEMPERATURA", 2);
    boxButton(270, 130, 108, 26, RED, NULL, selection == 0 ? true : false, "", "-  .C  +", 2);
    

    boxButton(50, 163, 108, 108, ORANGE, toleranceIcon, false, "", "TOLERANCIA", 2);
    boxButton(270, 245, 108, 26, ORANGE, NULL, selection == 1 ? true : false, "", "-  .C  +", 2);
}

void Display::humidMenu(int *menu)
{
    static int selection = 0;
    static float humid_value = dataClass->getTargetHumid();
    static float tol_value = dataClass->getHumidTolerance();

    if (selection < 0)
        selection = 0;
    if (selection > 1)
        selection = 1;

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Umidade Relativa");
    botScreen(labels);

    if(btn[0]->read())
    {
        if(selection == 0)
        {
            humid_value -= 0.5;
            if (humid_value > 100)
                humid_value = 0;
            if (humid_value < 0)
                humid_value = 100;
        }

        if(selection == 1)
        {
            tol_value -= 0.5;
            if (tol_value > 10)
                tol_value = 1;
            if (tol_value < 1)
                tol_value = 10;
        }
    }
    if(btn[1]->read())
    {
        if(selection == 0)
        {
            humid_value += 0.5;
            if (humid_value > 100)
                humid_value = 0;
            if (humid_value < 0)
                humid_value = 100;
        }

        if(selection == 1)
        {
            tol_value += 0.5;
            if (tol_value > 10)
                tol_value = 1;
            if (tol_value < 1)
                tol_value = 10;
        }
    }
    if(btn[2]->read())
    {
        if(selection == 0)
        {
            dataClass->setTargetHumid(humid_value);
            sendPacket(TH, humid_value);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid", humid_value);
        }
        if(selection == 1)
        {
            dataClass->setHumidTolerance(tol_value);
            sendPacket(HTOL, tol_value);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance", tol_value);
        }
        

        if(selection != 1)
            selection++;
        else
        {
            selection = 0;
            *menu = -1;        
        }
    }
    if(btn[3]->read())
    {
        if(selection == 0)
        {
            dataClass->setTargetHumid(humid_value);
            sendPacket(TH, humid_value);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid", humid_value);
        }
        if(selection == 1)
        {
            dataClass->setHumidTolerance(tol_value);
            sendPacket(HTOL, tol_value);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance", tol_value);
        }

        if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -1;
        }
    }

    String format_data[2] = {humid_value < 10 ? " " + String(humid_value,1) + " " : String(humid_value,1), 
                                tol_value < 10 ? " " + String(tol_value,1) + " " : String(tol_value,1)};

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);

    display->drawString(format_data[0], 320, 85, 8);
    display->drawString(format_data[1], 320, 200, 8);

    display->setTextDatum(TL_DATUM);
    boxButton(50, 48, 108, 108, PURPLE, humidIcon, false, "", "UMIDADE", 2);
    boxButton(270, 130, 108, 26, PURPLE, NULL, selection == 0 ? true : false, "", "-  %  +", 2);
    
    boxButton(50, 163, 108, 108, BLUE, toleranceIcon, false, "", "TOLERANCIA", 2);
    boxButton(270, 245, 108, 26, BLUE, NULL, selection == 1 ? true : false, "", "-  %  +", 2);
}

void Display::soilMenuSensor(int *menu)
{
    static int selection = 0;
    dataClass->getTargetSoil() > 100 ? dataClass->setTargetSoil(100) : dataClass->setTargetSoil(dataClass->getTargetSoil());

    static float soil_value = dataClass->getTargetSoil();
    static float pump_duration = dataClass->getPumpDuration();
    static float abs_delay = dataClass->getAbsorptionDelay();
    static float soil_tol = dataClass->getSoilTolerance();
    

    if (selection < 0)
        selection = 0;
    if (selection > 3)
        selection = 3;
    
    if (soil_value > 100)
        soil_value = 0;
    if (soil_value < 0)
        soil_value = 100;

    if (pump_duration > 300)
        pump_duration = 0;
    if (pump_duration < 0)
        pump_duration = 300;

    if (abs_delay > 300)
        abs_delay = 0;
    if (abs_delay < 0)
        abs_delay = 300;
    
    if (soil_tol > 10)
        soil_tol = 0;
    if (soil_tol < 0)
        soil_tol = 10;

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Rega e Bomba");
    botScreen(labels);

    if(btn[0]->read())
    {
        if(selection == 0)
        {
            soil_value -= 0.5;
            
        }
        if(selection == 1)
        {
            pump_duration -= 1;
            
        }
        if(selection == 2)
        {
            abs_delay -= 1;
            
        }
        if(selection == 3)
        {
            soil_tol -= 0.5;
            
        }
    }
    if(btn[1]->read())
    {
        if(selection == 0)
        {
            soil_value += 0.5;
            
        }
        if(selection == 1)
        {
            pump_duration += 1;
            
        }
        if(selection == 2)
        {
            abs_delay += 1;
            
        }
        if(selection == 3)
        {
            soil_tol += 0.5;
            
        }
    }
    if(btn[2]->read())
    {
        if (selection == 0)
        {
            dataClass->setTargetSoil(soil_value);
            sendPacket(TS, soil_value);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil", soil_value);
        }
        if (selection == 1)
        {
            dataClass->setPumpDuration(pump_duration);
            sendPacket(PD, pump_duration);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", pump_duration);
        }
        if (selection == 2)
        {
            dataClass->setAbsorptionDelay(abs_delay);
            sendPacket(AD, abs_delay);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", abs_delay);
        }
        if (selection == 3)
        {
            dataClass->setSoilTolerance(soil_tol);
            sendPacket(STOL, soil_tol);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance", soil_tol);
        }

        if(selection != 3)
            selection++;
        else
        {
            selection = 0;
            *menu = -1;        
        }
    }
    if(btn[3]->read())
    {
        if (selection == 0)
        {
            dataClass->setTargetSoil(soil_value);
            sendPacket(TS, soil_value);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil", soil_value);
        }
        if (selection == 1)
        {
            dataClass->setPumpDuration(pump_duration);
            sendPacket(PD, pump_duration);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", pump_duration);
        }
        if (selection == 2)
        {
            dataClass->setAbsorptionDelay(abs_delay);
            sendPacket(AD, abs_delay);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", abs_delay);
        }
        if (selection == 3)
        {
            dataClass->setSoilTolerance(soil_tol);
            sendPacket(STOL, soil_tol);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance", soil_tol);
        }

        if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -1;
        }
    }

    String format_data[4] = {{(soil_value < 10  || soil_value == 100) ? " " + String(int(soil_value)) + " " : String(soil_value, 1)}, 
                             {pump_duration < 10 ? "  " + String(int(pump_duration)) + "  " : String(int(pump_duration))}, 
                             {abs_delay < 10 ? "  " + String(int(abs_delay)) + "  " : String(int(abs_delay))}, 
                             {soil_tol < 10 ? " " + String(soil_tol, 1) + " " : String(soil_tol, 1)}};

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);

    display->drawString(format_data[0], 182, 95, 6);
    display->drawString(format_data[1], 182, 210, 6);
    display->drawString(format_data[2], 412, 95, 6);
    display->drawString(format_data[3], 412, 210, 6);

    display->setTextDatum(TL_DATUM);
    boxButton(16, 48, 108, 108, GREEN, soilIcon, false, "", "SOLO", 2);
    boxButton(130, 130, 108, 26, GREEN, NULL, selection == 0 ? true : false, "", "-  %  +", 2);
    
    boxButton(16, 163, 108, 108, BLUE, watchIcon, false, String((dataClass->getPumpFlow() * pump_duration) / 10) + "mL", "DURACAO", 2);
    boxButton(130, 245, 108, 26, BLUE, NULL, selection == 1 ? true : false, "", "-  S  +", 2);

    boxButton(246, 48, 108, 108, ORANGE, watchIcon, false, "SENSOR", "INTERVALO", 2);
    boxButton(360, 130, 108, 26, ORANGE, NULL, selection == 2 ? true : false, "", "-  S  +", 2);
    
    boxButton(246, 163, 108, 108, BROWN, toleranceIcon, false, "", "TOLERANCIA", 2);
    boxButton(360, 245, 108, 26, BROWN, NULL, selection == 3 ? true : false, "", "-  %  +", 2);

}

void Display::soilMenuTimer(int *menu)
{
    static int selection = 0;
    
    float abs_delay = dataClass->getAbsorptionDelay() / 3600;
    float pump_duration = dataClass->getPumpDuration();
    
    if (selection < 0)
        selection = 1;
    if (selection > 1)
        selection = 0;

    if (abs_delay > 24)
        abs_delay = 1;
    if (abs_delay < 1)
        abs_delay = 24;
    
    if (pump_duration > 300)
        pump_duration = 0;
    if (pump_duration < 0)
        pump_duration = 300;

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Rega e Bomba");
    botScreen(labels);

    if(btn[0]->read())
    {
        if(selection == 0)
        {
            abs_delay -= 1;
            dataClass->setAbsorptionDelay(abs_delay * 3600);
        }
        if(selection == 1)
        {
            pump_duration -= 1;
            dataClass->setPumpDuration(pump_duration);
        }
    }
    if(btn[1]->read())
    {
        if(selection == 0)
        {
            abs_delay += 1;
            dataClass->setAbsorptionDelay(abs_delay * 3600);
        }
        if(selection == 1)
        {
            pump_duration += 5;
            dataClass->setPumpDuration(pump_duration);
        }
    }
    if(btn[2]->read())
    {
        if(selection == 0)
        {
            dataClass->setAbsorptionDelay(abs_delay * 3600);
            float delay = dataClass->getAbsorptionDelay();
            sendPacket(AD, dataClass->getAbsorptionDelay());
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", delay);
            //Serial.printf("AD: %f\n", dataClass->getAbsorptionDelay());
        }
        if(selection == 1)
        {
            dataClass->setPumpDuration(pump_duration);
            sendPacket(PD, dataClass->getPumpDuration());
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", pump_duration);
            //Serial.printf("PD: %f\n", dataClass->getPumpDuration());
        }
        if(selection != 1)
            selection++;
        else
        {
            selection = 0;
            *menu = -1;        
        }
    }
    if(btn[3]->read())
   {
     if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -1;
        }
   }

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);

    display->drawString(abs_delay < 10 ? " " + String(int(abs_delay)) + " " : String(int(abs_delay)), 304, 85, 8);
    display->drawString(pump_duration < 10 ? "  " + String(int(pump_duration)) + "  " : String(int(pump_duration)), 304, 200, 8);
    
    display->setTextDatum(TL_DATUM);
    boxButton(50, 48, 108, 108, YELLOW, watchIcon, false, "TIMER", "INTERVALO", 2);
    boxButton(250, 130, 108, 26, YELLOW, NULL, selection == 0 ? true : false, "", "- HORA +", 2);
    

    boxButton(50, 163, 108, 108, BLUE, wateringIcon, false, String(dataClass->getPumpFlow() * pump_duration / 10) + "mL", "DURACAO", 2);
    boxButton(250, 245, 108, 26, BLUE, NULL, selection == 1 ? true : false, "", "- SEGS +", 2);
    

}

void Display::calibrationScreen(int *menu)
{
    static int submenu = -2;
    static int last_submenu = -2;

    if(submenu == -1)
    {
        submenu = -2;
        *menu = -1;
    }
    if (submenu > 4)
        submenu = -2;

   
    
   if(submenu != last_submenu)
   {
        drawBackGround();
        last_submenu = submenu;
   }
   

    switch (submenu)
    {
    
    case -2:
    {
        String label[4] = {"<", ">", "Selecionar", "Voltar"};
        topScreen("Menu Calibracao");
        botScreen(label);
        String labels[2][4] ={{"Sensor Solo", "Sensor Reserv.", "Bomba", "Rega"},{"Luz", "", "", ""}};
        int colors[2][4] = {{GREEN, YELLOW, RED, BLUE},{ORANGE, DARK_GREY, DARK_GREY, DARK_GREY}};
        const unsigned char *images[2][4] ={{soilIcon, waterReservIcon, pumpIcon, wateringIcon},{lightPeriodIcon, NULL, NULL, NULL}};
        buttonScreen(labels, colors, images, &submenu, 4);

        break;
    }
    case 0:

        soilSensorCalibScreen(&submenu);
        break;
    
    case 1:

        waterReservCalibScreen(&submenu);
        break;

    case 2:
        
        pumpCalibScreen(&submenu);
        break;

    case 3:
        
        wateringCalibScreen(&submenu);        
        break;

    case 4:
        
        lightCalibScreen(&submenu);
        break;
    
    default:
        break;
    }

}

void Display::soilSensorCalibScreen(int *menu)
{
    static int selection = 0;

    if (selection < 0)
        selection = 1;
    if (selection > 1)
        selection = 0;

    String label[4] = {"<", ">", "Confirmar", "Voltar"};
    topScreen("Calib. Sensor do solo");
    botScreen(label);
    
    String soil_low = String(int(dataClass->getSoil()));
    String soil_up = String(int(dataClass->getSoil()));

    if(btn[0]->read())
        selection--;
    if(btn[1]->read())
        selection++;
    if(btn[2]->read())
    {
        if (selection == 0)
        {
            float dry = 0;
            dataClass->setSoilLow(dataClass->getSoil());
            dry = dataClass->getSoil();
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry", dry);
            soil_low += " - OK";
        }

        if (selection == 1)
        {
            float wet = 0;
            dataClass->setSoilUpper(dataClass->getSoil());
            wet = dataClass->getSoil();
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet", wet);
            soil_up += " - OK"; 
        }

        if(selection != 1)
            selection++;
        else
        {
            selection = 0;
            *menu = -2;        
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -2;
        }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Retire o sensor do solo e coloque-o, com auxilio da haste, num copo", 20, 50, 2);
    display->drawString("vazio e aguarde a leitura se estabilizar, logo apos confirme ", 20, 67, 2);
    display->drawString("pressionando o botao verde.", 20, 84, 2);
    
    boxButton(80, 105, 150, 80, ORANGE, NULL, selection == 0 ? true : false, soil_low, "Seco", 4);
    boxButton(260, 105, 150, 80, BLUE, NULL, selection == 1 ? true : false, soil_up, "Umido", 4);

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Agora, insira o sensor no solo com a umidade desejada ate a marca.", 20, 190, 2);
    display->drawString("Tambem e possivel inserir num copo com agua, aguarde a leitura se", 20, 207, 2);
    display->drawString("estabilizar e pressione o botao verde.", 20, 224, 2);

    display->setTextColor(dataClass->isSoilCalibrated() ? GREEN : RED);
    display->drawString(dataClass->isSoilCalibrated() ? "Calibrado!" : "Nao calibrado!", 310, 240, 4);
    display->setTextColor(WHITE, DARK_GREY);


}
void Display::waterReservCalibScreen(int *menu)
{
    static int selection = 0;
    float level = dataClass->getWaterCapacity();

    if (selection < 0)
        selection = 1;
    if (selection > 1)
        selection = 0;

    if (level < 0)
        level = 0;
    if (level > 20)
        level = 0;

    String label[4] = { selection == 0 ? " < " : " - ", selection == 0 ? " > " : " + ", "Confirmar", "Voltar"};
    topScreen("Calib. Sensor Reservatorio");
    botScreen(label);

    String status = dataClass->getWaterRawReading() == -1 ? "" : " - OK";
    String water = String(dataClass->getWaterRes(), 1) + status;

    if(btn[0]->read())
    {
        if(selection == 0)
            selection--;
        if(selection == 1)
        {
            level -= 0.1;
            dataClass->setWaterCapacity(level);
        }
    }
    if(btn[1]->read())
    {
        if(selection == 0)
            selection++;
        if(selection == 1)
        {
            level += 0.5;
            dataClass->setWaterCapacity(level);
        }
    }
    if(btn[2]->read())
    {
        if(selection == 0)
        {
            float reserv = 0;
            dataClass->setWaterRawReading(dataClass->getWaterRes());
            reserv = dataClass->getWaterRawReading();
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Reserv", reserv);
        }

        if(selection == 1)
        {
            float capacity = 0;
            dataClass->setWaterCapacity(level);
            capacity = dataClass->getWaterCapacity();
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Capacity", capacity);
        }

        if(selection != 1)
            selection++;
        else
        {
            selection = 0;
            *menu = -2;        
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -2;
        }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Retire a agua do recipiente, feche a tampa e aguarde a leitura se", 20, 50, 2);
    display->drawString("estabilizar, logo apos confirme pressionando o botao verde.", 20, 67, 2);
   

    boxButton(160, 90, 160, 65, ORANGE, NULL, selection == 0 ? true : false, water, "Confirmar", 4);

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Caso saiba a capacidade do seu recipiente insira abaixo apos a", 20, 155, 2);
    display->drawString("conclusao da calibracao.", 20, 172, 2);
    

    boxButton(160, 208, 160, 65, BLUE, NULL, selection == 1 ? true : false, dataClass->getWaterCapacity() == NOT_DEFINED ? "Nao definido" : String(dataClass->getWaterCapacity()) , "- Litros +", 4);

}
void Display::pumpCalibScreen(int *menu)
{
    static int selection = 0;
    static float flow_value = 0;
    String status = String(dataClass->getPumpFlow());
    if (selection < 0)
        selection = 1;
    if (selection > 1)
        selection = 0;
    
    if(flow_value < 0)
        flow_value = 0;
    if(flow_value > 1000)
        flow_value = 1000;

    float pump_flow = dataClass->getPumpFlow();
    String label[4] = {selection == 0 ? "<" : " - ", selection == 0 ? ">" : " + ", selection == 0 ? "  Acionar  " : "Confirmar", "Voltar"};
    topScreen("Calib. Bomba de Agua");
    botScreen(label);

    if(btn[0]->read())
    {
        if(selection == 0)
            selection--;
        if(selection == 1)
        {
            flow_value -= 1;
            dataClass->setPumpFlow(flow_value);
        }
    }
    if(btn[1]->read())
    {
        if(selection == 0)
            selection++;
        if(selection == 1)
        {
            flow_value += 10;
            dataClass->setPumpFlow(flow_value);
        }
    }
    if(btn[2]->read())
    {
        if(selection == 0)
        {
            //status = "Aguarde";
            sendPacket(PUMP_CAL, true);
            delay(10000);
            sendPacket(PUMP_CAL, false);
        }
        if(selection != 1)
            selection++;
        else
        {
            flow_value = dataClass->getPumpFlow();
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow", flow_value);
            selection = 0;
            *menu = -2;        
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
        selection--;
        else
        {
            flow_value = dataClass->getPumpFlow();
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow", flow_value);
            selection = 0;
            *menu = -2;
        }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Coloque a saida da bomba ou soleinoide num recipiente graduado, aperte", 20, 50, 2);
    display->drawString("o botao e aguarde 10 segundos, depois informe o valor da leitura.", 20, 67, 2);
   
    boxButton(165, 90, 150, 65, ORANGE, NULL, true, selection == 0 ? status : String(dataClass->getPumpFlow()), "- mL +", 4);

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Outra alternativa e pesar a quantidade de agua despejada no recipiente", 20, 157, 2);
    display->drawString("e inserir seu valor da mesma forma do primeiro metodo.", 20, 174, 2);
    

    boxButton(165, 203, 150, 65, BLUE, NULL, false, String(dataClass->getPumpFlow() / 10), "mL/S", 4);
}
void Display::wateringCalibScreen(int *menu)
{
    static int selection = 0;
    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen("Ajuste Rega");
    botScreen(label);

    if (selection < 0)
        selection = 1;
    if (selection > 1)
        selection = 0;

    if(btn[0]->read())
    {
        selection--;
    }
    if(btn[1]->read())
    {
        selection++;
    }
    if(btn[2]->read())
    {
        if(selection == 0)
        {
            dataClass->setSoilBehavior(SENSOR);
            float behavior = SENSOR;
            
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Behavior", behavior);
            dataClass->getTargetSoil() == 150 ? dataClass->setTargetSoil(50) : dataClass->setTargetSoil(dataClass->getTargetSoil());
            dataClass->setAbsorptionDelay(300);
            float abs = 300;
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", abs);
            sendPacket(TS, dataClass->getTargetSoil());

        }
        if(selection == 1)
        {
            dataClass->setSoilBehavior(TIMER);
            float behavior = TIMER;
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Behavior", behavior);
            dataClass->setTargetSoil(150);
            dataClass->setAbsorptionDelay(3600);
            float abs = 3600;
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", abs);
            sendPacket(TS, dataClass->getTargetSoil());
        }
        // if(selection != 1)
        //     selection++;
        // else
        // {
            selection = 0;
            *menu = -2;        
        // }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -2;
        }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Escolha como sua bomba de rega sera ativada. Na opcao timer a ", 20, 50, 2);
    display->drawString("leitura do sensor permanece ativa mas nao influencia no", 20, 67, 2);
    display->drawString("comportamento da bomba.", 20, 84, 2);
   
    boxButton(40, 110, 150, 150, ORANGE, NULL, selection == 0 ? true : false, dataClass->getSoilBehavior() == SENSOR ? "OK" : "", "SENSOR", 4);
    boxButton(290, 110, 150, 150, BLUE, NULL, selection == 1 ? true : false, dataClass->getSoilBehavior() == TIMER ? "OK" : "", "TIMER", 4);

}
void Display::lightCalibScreen(int *menu)
{
    static int selection = 0;
    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen("Ajuste Luz");
    botScreen(label);

    if(btn[0]->read())
        ;
    if(btn[1]->read())
        ;
    if(btn[2]->read())
    {
       if(selection != 3)
        selection++;
        else
        {
            selection = 0;
            *menu = -2;        
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
        selection--;
        else
        {
            selection = 0;
            *menu = -2;
        }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Escolha como sua luz sera ativada. Na opcao timer a luz e ativada", 20, 50, 2);
    display->drawString("por um timer, ignorando o relogio. Na opcao relogio, o comportamento", 20, 67, 2);
    display->drawString("da luz sera ditado pelo relogio do equipamento.", 20, 84, 2);
   
    boxButton(40, 110, 150, 100, ORANGE, NULL, false, "", "RELOGIO", 4);
    boxButton(290, 110, 150, 100, BLUE, NULL, false, "", "TIMER", 4);

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Na opcao timer ha uma ligeira perda de precisao com a medicao de", 20, 217, 2);
    display->drawString("tempo na casa de segundos e ate minutos por dia.", 20, 234, 2);
}

void Display::setupScreen(int *menu)
{
    static int submenu = -2;
    static int last_submenu = -2;

    if(submenu == -1)
    {
        submenu = -2;
        *menu = -1;
    }
    if (submenu > 3)
        submenu = -2;
    
    if(submenu != last_submenu)
    {
        drawBackGround();
        last_submenu = submenu;
    }

    switch (submenu)
    {

        case -2:
        {    
            String label[4] = {"<", ">", "Selecionar", "Voltar"};
            topScreen("Menu Configuracao");
            botScreen(label);
            String labels[2][4] ={{"WiFi", "Relogio", "Atualizacoes", "Sobre"},{"", "", "", ""}};
            int colors[2][4] = {{GREEN, YELLOW, RED, ORANGE},{DARK_GREY, DARK_GREY, DARK_GREY, DARK_GREY}};
            const unsigned char *images[2][4] ={{wifiIcon, watchIcon, updateIcon, aboutIcon},{NULL, NULL, NULL, NULL}};
            buttonScreen(labels, colors, images, &submenu, 3);

            break;
        }

        case 0:
            wifiConfScreen(&submenu);
            break;
        
        case 1:
            clockConfScreen(&submenu);
            break;

        case 2:
            updateConfScreen(&submenu);
            break;

        case 3:
            aboutConfScreen(&submenu);
            break;
        
        default:
            break;
        }
}


void Display::wifiConfScreen(int *menu)
{
    static int selection = 0;

    if (selection < 0)
        selection = 1;
    if (selection > 1)
        selection = 0;

    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen("Config. WiFi");
    botScreen(label);

    if(btn[0]->read())
        selection--;
    if(btn[1]->read())
        selection++;
    if(btn[2]->read())
    {
        if(selection == 0)
        {
            wifi->wifiInit();
        }
        
        if(selection == 1)
            wifi->startPortal();

        if(selection != 2)
            selection++;
        else
        {
            selection = 0;
            *menu = -2;        
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
            selection--;
        else
        {
            selection = 0;
            *menu = -2;
        }
    }

    String SSID = wifi->getStatus() ? wifi->getSSID() : "Clique e aguarde para conectar";
    
    boxButton(40, 50, 400, 100, GREEN, NULL, selection == 0 ? true : false, SSID, "CONECTAR", 4);
    boxButton(40, 170, 400, 100, BLUE, NULL, selection == 1 ? true : false, "TROCAR CREDENCIAIS", "O dispositivo ira reiniciar", 4);

}
void Display::clockConfScreen(int *menu)
{
    static int selection = 0;
    static int timezone = rtc->getTimeZone() / 3600;

    if(timezone < -12)
        timezone = 14;
    if(timezone > 14)
        timezone = -12;

    if (selection < 0)
        selection = 1;
    if (selection > 1)
        selection = 0;

    
    if (selection == 0)
    {
        String label[4] = {" < ", " > ", "Selecionar", "Voltar"};
        botScreen(label);
    }
    if (selection == 1)
    {
        String label[4] = {" - ", " + ", "Confirmar", "Voltar"};
        botScreen(label);
    }
    topScreen("Config. Relogio");

    String hour = "";
    static bool is_ok = false;
    static String status = is_ok ? hour : "Selecione e aguarde";
    static String status2 = "Funciona apenas sem conexao";

    if(btn[0]->read())
    {
        if (selection == 0)
            selection--;
        if (selection == 1)
        {
            timezone--;
            rtc->setgmtOffSet(timezone);
        }

    }
    if(btn[1]->read())
    {
        if (selection == 0)
            selection++;
        if (selection == 1)
        {
            timezone++;
            rtc->setgmtOffSet(timezone);
        }
    }
    if(btn[2]->read())
    {
        if(selection == 0)
        {
            if(wifi->getStatus())
            {
                status = "Aguarde...";
                is_ok = rtc->begin();
            }
            else
                status = "Conecte a internet!";
        }

        if(selection == 1)
        {
            if(!wifi->getStatus())
                status2 = "Conecte a internet!";
            else
                selection = 0;
                *menu = -2;   
        }

        if(selection != 2)
            selection++;
        else
        {
            status = "Selecione e aguarde";
            selection = 0;
            *menu = -2;        
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
            selection--;
        else
        {
            status = "Selecione e aguarde";
            selection = 0;
            *menu = -2;
        }
    }

    
    hour = is_ok ? (rtc->getHour() < 10 ? "0" + String(rtc->getHour()) : String(rtc->getHour())) + " : " + 
                   (rtc->getMinute() < 10 ? "0" + String(rtc->getMinute()) : String(rtc->getMinute())) + " : " + 
                   (rtc->getSecond() < 10 ? "0" + String(rtc->getSecond()) : String(rtc->getSecond())) : "";

    boxButton(40, 50, 400, 100, GREEN, NULL, selection == 0 ? true : false, is_ok ? hour : status, "SINCRONIZAR", 4);
    boxButton(40, 170, 400, 100, BLUE, NULL, selection == 1 ? true : false, String(rtc->getTimeZone()) + "Hrs", "FUSO HORARIO", 4);
}
void Display::updateConfScreen(int *menu)
{
    static int selection = 0;
    
    
    if (selection < 0)
        selection = 1;
    if (selection > 1)
        selection = 0;

    String label[4] = {" ", " ", selection == 0 ? "Verificar" : "Atualizar", "Voltar"};
    topScreen("Config. Atualizacoes");
    botScreen(label);

    if(btn[0]->read())
        ;
    if(btn[1]->read())
        ;
    if(btn[2]->read())
    {
        ota->fetchReleaseInfo();
        if(selection == 1 && (ota->getVersion() != ota->getReleaseTag()))
            ota->updateDevice();
        if(selection != 1)
            selection++;
        else
        {
            selection = 0;
            *menu = -2;        
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
        selection--;
        else
        {
            selection = 0;
            *menu = -2;
        }
    }


    boxButton(40, 50, 400, 100, GREEN, NULL, true, "Versao atual: " + ota->getVersion(), selection == 0 ? " VERIFICAR " : "ATUALIZAR", 4);
    display->setTextColor(WHITE, DARK_GREY);
    if(ota->getVersion() != ota->getReleaseTag())
    {
        display->drawString("Notas da nova versao: " + ota->getReleaseTag() + " - " + ota->getReleaseDate(), 40, 160, 2);
        String notes[2] = {ota->getReleaseName(), ota->getReleaseBody()};
        for(int i = 0; i < 2; i++)
            display->drawString(notes[i], 40, 175 +(15 * i), 2);
    }
    else 
        display->drawString("Nao ha novas atualizacoes no momento.", 40, 160, 2);
}

void Display::aboutConfScreen(int *menu)
{
    static int selection = 0;

    if (selection < 0)
        selection = 0;
    if (selection > 3)
        selection = 3;

    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen("Sobre");
    botScreen(label);

    if(btn[0]->read())
        ;
    if(btn[1]->read())
        ;
    if(btn[2]->read())
    {
       if(selection != 3)
        selection++;
        else
        {
            selection = 0;
            *menu = -2;        
        }
    }
    if(btn[3]->read())
    {
        if(selection != 0)
        selection--;
        else
        {
            selection = 0;
            *menu = -2;
        }
    }
}

void Display::connectionScreen(String label_1, String label_2)
{
    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);
    display->drawString(label_1, 240, 25, 4);
    //display->pushImage(110, 0, 260, 320, logoIcon);
    display->drawString(label_2, 240, 300, 4);
}




void Display::wifiConnStatus()
{
    static unsigned long lastUpdate = 0;
    static int lastSignal = 0;
    static int smoothedRSSI = -70;
    
    unsigned long now = millis();
    bool needsUpdate = (now - lastUpdate >= 10000);
    
    // Constantes fixas
    const int initX = 20;
    const int initY = 13;  // FIXO em ambos os casos
    const int barW = 5;
    const int space = 1;
    const int maxH = 14;
    
    int statusConnColor = wifi->getStatus() ? WHITE : RED;
    int signal = lastSignal;
    
    if (needsUpdate) 
    {
        lastUpdate = now;
        
        int currentRSSI = wifi->getSignalStrenght();
        
        // Suavização com inteiros
        smoothedRSSI = (int)((currentRSSI * 0.7f) + (smoothedRSSI * 0.3f));
        
        if (!wifi->getStatus()) {
            signal = 0;
        }
        else if (smoothedRSSI >= -55) signal = 5;
        else if (smoothedRSSI >= -65) signal = 4;
        else if (smoothedRSSI >= -75) signal = 3;
        else if (smoothedRSSI >= -85) signal = 2;
        else if (smoothedRSSI >= -95) signal = 1;
        else signal = 0;
        
        lastSignal = signal;
    }
    
    // Limpa a área UMA VEZ
    display->fillRect(initX, initY, (barW + space) * 5, maxH, BLACK);
    
    // Desenha as barras
    for (int i = 0; i < 5; i++)
    {
        int h = (i + 1) * 3;
        int color = (i < signal) ? statusConnColor : DARK_GREY;
        
        display->fillRect(
            initX + i * (barW + space),
            initY + (maxH - h),
            barW,
            h,
            color
        );
    }
}

void Display::qrScreen()
{
    display->setTextDatum(MC_DATUM);
    display->fillScreen(BG_ICON);
    display->setTextColor(WHITE, BG_ICON);
    display->drawBitmap(165, 50, wifiqr, 150, 150, WHITE);
    display->drawString("Para configurar sua rede e credenciais, aponte seu celular", 240, 255, 2);
    display->drawString("para este QR Code ou procure por Growbox na sua rede WiFi.", 240, 270, 2);
    display->setTextDatum(TL_DATUM);
   
}

void Display::waitBox()
{
    display->setTextDatum(MC_DATUM);
    display->setTextColor(WHITE, DARK_GREY);
    display->drawRoundRect(190, 160, 100, 60, 3, WHITE);
    display->fillRoundRect(190, 160, 100, 60, 3, DARK_GREY);
    display->drawString("Aguarde...",240, 160, 2);
}

