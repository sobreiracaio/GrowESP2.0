#include "Display.hpp"

Display::Display(TFT_eSPI *tft, FBase *fbase, Time *time, OTA *ota,
                 WifiManager *wifi_manager, Button **button, DataClass *data_class)
    : display(tft), firebase(fbase), rtc(time), ota(ota),
      wifi(wifi_manager), btn(button), dataClass(data_class)
{
    for (int i = 0; i < 2; i++) { day[i] = 0; night[i] = 0; }
    memset(gradientCache, 0, sizeof(gradientCache));
    gaugeSprite = new TFT_eSprite(display);
    gaugeSprite->createSprite(110, 120);
    gaugeSprite->setColorDepth(16);
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

// ── Display health check ──────────────────────────────────────────────────────
// ST7796: comando 0x0A (Read Display Power Mode)
// Byte saudavel: 0x9C (Sleep Out + Display On + Normal Mode)
// 0x00 ou 0xFF indicam falha de conexao fisica ou controlador morto.
bool Display::isDisplayHealthy()
{
    uint8_t mode = display->readcommand8(0x0A);
    return (mode != 0x00 && mode != 0xFF);
}

// Tenta ressuscitar o display ate MAX_ATTEMPTS vezes.
// Se recuperar, redesenha o fundo e retorna true.
// Se nao recuperar, loga e retorna false — o sistema segue sem travar.
bool Display::recoverDisplay()
{
    const int MAX_ATTEMPTS = 3;
    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        Serial.printf("[DISPLAY] Tentativa de recuperacao %d/%d...\n", i+1, MAX_ATTEMPTS);
        display->init();
        display->setRotation(1);
        display->setSwapBytes(true);
        // Não acende tela aqui — brightness é controlado pelo ButtonIdle
        esp_task_wdt_reset();
        if (isDisplayHealthy()) {
            Serial.println("[DISPLAY] Recuperado com sucesso.");
            display->fillScreen(BLACK);
            drawBackGround();
            return true;
        }
    }
    Serial.println("[DISPLAY] Falha permanente — sem recuperacao.");
    return false;
}

// Chama quando a tela esta apagada — intervalo de 1 hora.
// Leitura tripla sem delay — apenas reset do WDT entre leituras.
// Nao chamar com tela acesa para evitar interferencia visual.
void Display::healthCheck()
{
    static unsigned long lastCheck = 0;
    unsigned long now = millis();

    if (now - lastCheck < 3600000UL) return;
    lastCheck = now;

    // Leitura tripla — considera falha apenas se 2 de 3 leituras forem ruins
    int falhas = 0;
    for (int i = 0; i < 3; i++) {
        uint8_t mode = display->readcommand8(0x0A);
        if (mode == 0x00 || mode == 0xFF) falhas++;
        esp_task_wdt_reset();
    }

    if (falhas >= 2) {
        Serial.printf("[DISPLAY] Anomalia detectada (%d/3 leituras ruins), tentando recuperar...\n", falhas);
        recoverDisplay();
    }
}

void Display::injectFBase(FBase *fbase) { firebase = fbase; }

static bool _buttonScreenNeedsRedraw = true;

void Display::invalidateButtonScreen()
{
    _buttonScreenNeedsRedraw = true;
}

// Reseta o timer interno do pulseColorTimed — chame após setup longo
// para evitar salto de animação no primeiro frame do menu
static bool _pulseNeedsReset = false;

void Display::resetPulseTimer()
{
    _pulseNeedsReset = true;
}

// ── Helpers de envio com verificação de mudança ───────────────────────────────
// Só faz a requisição HTTP se o valor realmente mudou em relação ao getter atual
void Display::_setFloatIfChanged(const String& path, float newVal, float curVal)
{
    if (fabsf(newVal - curVal) < 0.001f) return;
    firebase->aSyncSetFloat(path, newVal);
}

void Display::_setStringIfChanged(const String& path, const String& newVal, const String& curVal)
{
    if (newVal == curVal) return;
    firebase->aSyncSetString(path, newVal);
}

void Display::_setBoolIfChanged(const String& path, bool newVal, bool curVal)
{
    if (newVal == curVal) return;
    firebase->aSyncSetBool(path, newVal);
}

void Display::logoScreen(String text)
{
    display->fillScreen(BG_ICON);
    display->setTextColor(WHITE, BG_ICON);
    display->setTextDatum(MC_DATUM);
    display->drawString(text, 240, 300, 2);
}

void Display::drawBackGround(int color)
{
    display->fillScreen(BLACK);
    display->fillRoundRect(5, 40, 470, 240, 10, color);
}

// ── waitBox ───────────────────────────────────────────────────────────────────
// show=true  → balão semi-transparente com pontilhado sobre o fundo atual
// show=false → apaga redesenhando o fundo sólido na região
void Display::waitBox(bool show)
{
    const int bx = 160, by = 155, bw = 160, bh = 50, br = 6;

    if (!show) {
        display->fillRoundRect(bx, by, bw, bh, br, DARK_GREY);
        return;
    }

    // Pontilhado: pixels alternados em DARK_GREY simulam ~50% de transparência
    // (px+py) % 2 == 0 → pinta, caso contrário deixa o pixel do fundo intacto
    for (int py = by; py < by + bh; py++)
        for (int px = bx; px < bx + bw; px++)
            if ((px + py) % 2 == 0)
                display->drawPixel(px, py, DARK_GREY);

    // Borda branca por cima do pontilhado
    display->drawRoundRect(bx,     by,     bw,     bh,     br, WHITE);
    display->drawRoundRect(bx + 1, by + 1, bw - 2, bh - 2, br, WHITE);

    // Fundo sólido apenas na área do texto para garantir legibilidade
    const char* label = "Aguarde...";
    int tw = display->textWidth(label, 2);
    int th = display->fontHeight(2);
    int tx = bx + (bw - tw) / 2;
    int ty = by + (bh - th) / 2;
    const int pad = 4;
    display->fillRect(tx - pad, ty - pad, tw + pad * 2, th + pad * 2, DARK_GREY);

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(TL_DATUM);
    display->drawString(label, tx, ty, 2);
}

// ── boxButton ────────────────────────────────────────────────────────────────
// Ordem: limpeza → borda → texto
// A limpeza é feita com dois fillRect laterais ANTES da borda, assim a borda
// pulsante nunca sobrepõe o texto e não há flicker.
// Altura dos rects = fontHeight + 2px, clampada à área interna da caixa.
void Display::boxButton(int x, int y, int w, int h, int color,
                        const uint16_t *image,
                        bool is_selected,
                        String text_top, String text_bot, int text_size)
{
    int pulsed_color = is_selected
        ? pulseColorTimed(color, DARK_GREY, text_size)
        : color;

    const int borderMargin = 6;
    int innerX    = x + borderMargin;
    int innerW    = w - borderMargin * 2;
    int innerYtop = y + borderMargin;
    int innerYbot = y + h - borderMargin;

    // ── helper: dois rects laterais clampados ────────────────────────────────
    auto clearSides = [&](int textX, int textW, int textY, int textH) {
        int rectY = textY - 1;
        int rectH = textH + 2;
        if (rectY < innerYtop)            rectY = innerYtop;
        if (rectY + rectH > innerYbot)    rectH = innerYbot - rectY;
        if (rectH <= 0) return;

        int leftW = textX - innerX;
        if (leftW > 0)
            display->fillRect(innerX, rectY, leftW, rectH, DARK_GREY);

        int rightStart = textX + textW;
        int rightW     = (innerX + innerW) - rightStart;
        if (rightW > 0)
            display->fillRect(rightStart, rectY, rightW, rectH, DARK_GREY);
    };

    // Calcula posição vertical dos textos
    // Caixas grandes: text_top no topo, text_bot no rodapé
    // Caixas pequenas (sem imagem): ambos centralizados verticalmente
    int th = display->fontHeight(text_size);
    bool smallBox = (h < 60) || (image == NULL && text_top.length() == 0);

    int ty_top, ty_bot;
    if (smallBox) {
        // Centraliza verticalmente — único texto (text_bot) no meio da caixa
        ty_top = y + (h - th) / 2;
        ty_bot = y + (h - th) / 2;
    } else {
        ty_top = y + 10;
        ty_bot = y + (h - th) - 5;
    }

    // Clamp para nunca sair da área interna
    if (ty_top < innerYtop) ty_top = innerYtop;
    if (ty_bot < innerYtop) ty_bot = innerYtop;
    if (ty_top + th > innerYbot) ty_top = innerYbot - th;
    if (ty_bot + th > innerYbot) ty_bot = innerYbot - th;

    // Borda pulsante
    for (int i = 0; i < 4; i++)
        display->drawRoundRect(x+i, y+i, w-(i*2), h-(i*2), 10-i, pulsed_color);

    display->setTextColor(color, DARK_GREY);

    if (smallBox) {
        // Caixas pequenas: texto fixo, sem limpeza lateral — evita flicker
        {
            int tw = display->textWidth(text_top, text_size);
            display->drawString(text_top, x + (w - tw) / 2, ty_top, text_size);
        }
        {
            int tw = display->textWidth(text_bot, text_size);
            display->drawString(text_bot, x + (w - tw) / 2, ty_bot, text_size);
        }
    } else {
        // Caixas grandes: limpeza lateral antes do texto para eliminar rebarbas
        {
            int tw = display->textWidth(text_top, text_size);
            clearSides(x + (w - tw) / 2, tw, ty_top, th);
            display->drawString(text_top, x + (w - tw) / 2, ty_top, text_size);
        }
        {
            int tw = display->textWidth(text_bot, text_size);
            clearSides(x + (w - tw) / 2, tw, ty_bot, th);
            display->drawString(text_bot, x + (w - tw) / 2, ty_bot, text_size);
        }
    }

    if (image != NULL) {
        // Normaliza pixels de borda/antialiasing para chroma key 0x2FE0
        // Range: R<=6 AND (G>=13 OR (G>=2 AND B<=2))
        static uint16_t imgBuf[64 * 64];
        for (int i = 0; i < 64 * 64; i++) {
            uint16_t col = pgm_read_word(&image[i]);
            uint8_t r = (col >> 11) & 0x1F;
            uint8_t g = (col >> 5)  & 0x3F;
            uint8_t b =  col        & 0x1F;
            imgBuf[i] = (r <= 6 && (g >= 13 || (g >= 2 && b <= 2))) ? 0x2FE0 : col;
        }
        display->pushImage(x + (w-64)/2, y + 25, 64, 64, imgBuf, (uint16_t)0x2FE0);
    }
}

void Display::buttonScreen(String labels[2][4], int colors[2][4],
                           const uint16_t *images[2][4],
                           int *menu, int menu_nbr)
{
    int distance_h = 113;
    int distance_v = 115;
    static int selection = 0;
    static int lastSelection = -99;
    static int lastMenuNbr = -1;
    bool forceRedraw = _buttonScreenNeedsRedraw;
    int is_selected[2][4] = {{0,1,2,3},{4,5,6,7}};

    // Se menu_nbr mudou (ex: calibração→configurações), reseta seleção e força redraw
    if (lastMenuNbr != menu_nbr) {
        selection = 0;
        lastSelection = -99;
        forceRedraw = true;
        lastMenuNbr = menu_nbr;
    }

    if (selection < 0) selection = menu_nbr;
    if (selection > menu_nbr) selection = 0;

    bool changed = false;
    if (btn[0]->read()) { selection--; changed = true; }
    if (btn[1]->read()) { selection++; changed = true; }
    if (btn[2]->read()) { *menu = selection; forceRedraw = true; }
    if (btn[3]->read()) {
        if (selection != 0) selection = 0;
        else *menu = -1;
        changed = true;
        forceRedraw = true;
    }

    bool needsFullRedraw = forceRedraw || (lastSelection != selection) || changed;

    // idx percorre linha a linha (j=0 primeiro, depois j=1)
    // para que menu_nbr corresponda à ordem visual da grade
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 4; i++) {
            int idx = j * 4 + i;
            if (idx > menu_nbr) continue;
            bool sel = (selection == is_selected[j][i]);
            bool prevSel = (lastSelection == is_selected[j][i]);
            int bx = 16 + (distance_h * i);
            int by = 48 + (distance_v * j);

            if (needsFullRedraw) {
                boxButton(bx, by, 108, 108, colors[j][i], images[j][i], sel, "", labels[j][i]);
            } else {
                if (sel || prevSel) {
                    boxButton(bx, by, 108, 108, colors[j][i], images[j][i], sel, "", labels[j][i]);
                }
            }
        }
    }

    if (needsFullRedraw) {
        lastSelection = selection;
        forceRedraw = false;
    }
}

uint16_t Display::pulseColorTimed(uint16_t c1, uint16_t c2, float speed)
{
    static float t = 0.0f;
    static int dir = 1;
    static unsigned long last = 0;
    static bool _pulseNeedsReset = false;

    if (_pulseNeedsReset) {
        last = millis();
        _pulseNeedsReset = false;
    }

    unsigned long now = millis();
    float dt = (now - last) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;  // cap apenas no primeiro frame após reset
    last = now;
    t += dir * speed * dt;

    if (t >= 1.0f) { t = 1.0f; dir = -1; }
    if (t <= 0.0f) { t = 0.0f; dir =  1; }

    uint8_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
    uint8_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;

    return ((uint8_t)(r1 + (r2-r1)*t) << 11) |
           ((uint8_t)(g1 + (g2-g1)*t) << 5)  |
            (uint8_t)(b1 + (b2-b1)*t);
}

void Display::drawAntenna()
{
    int col = wifi->getStatus() ? GREEN : RED;
    display->drawLine(55, 12, 55, 25, col);
    display->drawLine(50, 12, 55, 17, col);
    display->drawLine(60, 12, 55, 17, col);
    display->drawLine(58, 22, 61, 25, wifi->getStatus() ? BLACK : RED);
    display->drawLine(61, 22, 58, 25, wifi->getStatus() ? BLACK : RED);
}

void Display::topScreen(String label)
{
    char hour[3], minute[3];
    int h = (wifi->getStatus() && rtc->getHour()   != -1) ? rtc->getHour()   : 0;
    int m = (wifi->getStatus() && rtc->getMinute() != -1) ? rtc->getMinute() : 0;
    snprintf(hour,   sizeof(hour),   "%02d", h);
    snprintf(minute, sizeof(minute), "%02d", m);
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%s:%s", hour, minute);

    display->setTextColor(WHITE, BLACK);
    display->setTextDatum(MC_DATUM);
    display->drawString(label, 240, 20, 4);
    display->setTextDatum(TL_DATUM);
    wifiConnStatus();
    drawAntenna();
    display->drawString(timeStr, 410, (40 - display->fontHeight(2)) / 2, 4);
}

uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t interpolateColor(uint16_t c1, uint16_t c2, float t)
{
    uint8_t r1 = ((c1>>11)&0x1F)*255/31, g1 = ((c1>>5)&0x3F)*255/63, b1 = (c1&0x1F)*255/31;
    uint8_t r2 = ((c2>>11)&0x1F)*255/31, g2 = ((c2>>5)&0x3F)*255/63, b2 = (c2&0x1F)*255/31;
    return color565(r1+(r2-r1)*t, g1+(g2-g1)*t, b1+(b2-b1)*t);
}

uint16_t getGradientColor(float angle, uint16_t c1=RED, uint16_t c2=YELLOW, uint16_t c3=GREEN)
{
    angle = constrain(angle, 0, 270);
    if (angle <= 135) return interpolateColor(c1, c2, angle/135.0f);
    return interpolateColor(c2, c3, (angle-135)/135.0f);
}

void Display::drawArcGauge(int x, int y, float value, float targetValue,
                            float ceiling_value, float tolerance,
                            String label, String unit, int option, int decimals)
{
    if (!gradientCached) {
        for (int a = 0; a < 270; a++) {
            gradientCache[0][a] = getGradientColor(a);
            gradientCache[1][a] = getGradientColor(a, BLUE, YELLOW, RED);
            gradientCache[2][a] = getGradientColor(a, YELLOW, BLUE, PURPLE);
        }
        gradientCached = true;
    }

    // map() do Arduino usa long — usa fmap() float para valores decimais como VPD
    auto fmap = [](float x, float in_min, float in_max, float out_min, float out_max) -> float {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    };
    float angle       = constrain(fmap(value,              0, ceiling_value, 0, 270), 0.0f, 270.0f);
    float targetAngle1 = constrain(fmap(targetValue-tolerance, 0, ceiling_value, 0, 270), 1.0f, 269.0f);
    float targetAngle2 = constrain(fmap(targetValue+tolerance, 0, ceiling_value, 0, 270), 1.0f, 269.0f);

    int radius = 43;
    if (value > ceiling_value) value = ceiling_value;

    String fValue  = value      >= 100 ? String(int(value))      : String(value,  decimals);
    String fTarget = targetValue >= 100 ? String(int(targetValue)) : String(targetValue, decimals);

    gaugeSprite->fillSprite(DARK_GREY);
    int cx = 55, cy = 55;
    int step = 15;
    int opt_idx = option == 1 ? 1 : (option == 2 ? 2 : 0);

    for (int a = 0; a <= (int)angle; a += step) {
        int endAngle = min(a + step, (int)angle);
        gaugeSprite->drawArc(cx, cy, radius, 33, a, endAngle,
                             gradientCache[opt_idx][a], DARK_GREY);
    }
    gaugeSprite->drawArc(cx, cy, radius, 33, angle, 270, DARK_GREY, DARK_GREY);
    gaugeSprite->drawArc(cx, cy, radius+5, 45, targetAngle1, targetAngle2, YELLOW, DARK_GREY);

    gaugeSprite->setTextColor(WHITE, DARK_GREY);
    gaugeSprite->setTextDatum(MC_DATUM);
    gaugeSprite->drawString(fValue,  cx, cy,      4);
    gaugeSprite->drawString(fTarget, cx, cy+15,   2);
    gaugeSprite->drawString(unit,    cx+radius-20, cy+radius-10, 2);
    gaugeSprite->drawString(label,   cx, cy+radius+15, 2);
    gaugeSprite->pushSprite(x-cx, y-cy);
}

void Display::botScreen(String labels[4])
{
    int distance = 113;
    display->setTextColor(WHITE, BLACK);
    display->setTextDatum(MC_DATUM);

    display->fillCircle(68,               307, 8, WHITE);
    display->drawString(labels[0], 69,               290, 2);
    display->fillCircle(68+distance,      307, 8, WHITE);
    display->drawString(labels[1], 69+distance,      290, 2);
    display->fillCircle(68+distance*2,    307, 8, GREEN);
    display->drawString(labels[2], 69+distance*2,    290, 2);
    display->fillCircle(68+distance*3,    307, 8, RED);
    display->drawString(labels[3], 69+distance*3,    290, 2);
    display->setTextDatum(TL_DATUM);
}

void Display::mainScreen(int *menu)
{
    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen();
    botScreen(label);
    String labels[2][4] = {{"Monitorar","Fotoperiodo","Temperatura","Umidade"},
                            {"Rega","Calibrar","Configuracoes",""}};
    int colors[2][4] = {{WHITE,YELLOW,RED,BLUE},{BROWN,ORANGE,PURPLE,DARK_GREY}};
    const uint16_t *images[2][4] = {{monitorIcon,lightPeriodIcon,tempIcon,humidIcon},
                                          {wateringIcon,calibIcon,confIcon,NULL}};
    buttonScreen(labels, colors, images, menu, 6);
}

void Display::monitorMenu(int *menu)
{
    bool is_running = dataClass->getIsRunning();
    static int selection = 0;
    if (selection < 0) selection = 0;
    if (selection > 3) selection = 3;

    String labels[4] = {" ", " ", is_running ? " Parar " : " Iniciar ", "Voltar"};
    topScreen("Monitoramento");
    botScreen(labels);

    if (btn[2]->read()) {
        bool prev_running = dataClass->getIsRunning();
        is_running = !prev_running;
        dataClass->setIsRunning(is_running);
        sendPacket(STATUS0, is_running);
        waitBox(true);
        _setBoolIfChanged(safeEmail + "/Status", is_running, prev_running);
        dataClass->saveToPrefs();
        waitBox(false);
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { selection = 0; *menu = -1; }
    }

    int distance = 119;
    static const char* gauge_labels[4] = {"Temperatura","Umidade","VPD","Reserv."};
    static const char* gauge_units[4]  = {".C","%","kPa","%"};
    float gauge_values[4]     = {dataClass->getTemp(), dataClass->getHumid(),
                                  dataClass->getVPD(), dataClass->getWaterCalibrated()};
    float gauge_Tvalues[4]    = {dataClass->getTargetTemp(), dataClass->getTargetHumid(),
                                  0, 0};
    float gauge_Cvalues[4]    = {50, 100, 5, 100};
    float gauge_tolerances[4] = {dataClass->getTempTolerance(), dataClass->getHumidTolerance(),
                                  0, 0};
    int gauge_options[4] = {1, 2, 1, 2};

    static const int gauge_decimals[4] = {1, 1, 2, 1};
    for (int i = 0; i < 4; i++)
        drawArcGauge(60+(distance*i), 95, gauge_values[i], gauge_Tvalues[i],
                     gauge_Cvalues[i], gauge_tolerances[i],
                     gauge_labels[i], gauge_units[i], gauge_options[i], gauge_decimals[i]);

    display->setTextDatum(TL_DATUM);
    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Resumo dos perifericos", 120, 165, 4);

    int distance_y = 30, distance_x = 230;
    static const String devices[2][3] = {{"Luz","Bomba","Cooler"},{"Aquecedor","Umid.","Desumid."}};
    String status[2][3] = {
        {dataClass->getLightStatus()   ? "ON  " : "OFF",
         dataClass->getPumpStatus()    ? "ON  " : "OFF",
         dataClass->getCoolerStatus()  ? "ON  " : "OFF"},
        {dataClass->getHeaterStatus()  ? "ON  " : "OFF",
         dataClass->getHumidStatus()   ? "ON  " : "OFF",
         dataClass->getDehumidStatus() ? "ON  " : "OFF"}
    };
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 3; i++) {
            display->drawString(devices[j][i] + " ->", 30+distance_x*j, 195+distance_y*i, 2);
            display->setTextColor(status[j][i]=="ON  " ? GREEN : RED, DARK_GREY);
            display->drawString(status[j][i], 120+distance_x*j, 195+distance_y*i, 2);
            display->setTextColor(WHITE, DARK_GREY);
        }
}

void Display::lightMenu(int *menu)
{
    static int selection = 0;
    if (selection < 0) selection = 0;
    if (selection > 3) selection = 3;

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Fotoperiodo");
    botScreen(labels);

    auto adjustVal = [](int &v, int delta, int lo, int hi) {
        v += delta;
        if (v > hi) v = lo;
        if (v < lo) v = hi;
    };

    if (btn[0]->read() || btn[0]->held()) {
        if (selection == 0) adjustVal(day[0],   -1, 0, 23);
        if (selection == 1) adjustVal(day[1],   -1, 0, 59);
        if (selection == 2) adjustVal(night[0], -1, 0, 23);
        if (selection == 3) adjustVal(night[1], -1, 0, 59);
    }
    if (btn[1]->read() || btn[1]->held()) {
        if (selection == 0) adjustVal(day[0],   +1, 0, 23);
        if (selection == 1) adjustVal(day[1],   +1, 0, 59);
        if (selection == 2) adjustVal(night[0], +1, 0, 23);
        if (selection == 3) adjustVal(night[1], +1, 0, 59);
    }

    auto buildHour = [](int h, int m) -> String {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
        return String(buf);
    };

    auto confirmLight = [&](int sel) {
        String hour;
        if (sel == 0 || sel == 1) {
            int d[2]; dataClass->getDayTime(d);
            hour = buildHour(day[0], day[1]);
            String prevHourOn = buildHour(d[0], d[1]);
            dataClass->setDayTime(day[0], day[1]);
            waitBox(true);
            _setStringIfChanged(safeEmail + "/InsertedData/Light/HourOn", hour, prevHourOn);
            waitBox(false);
        } else {
            int n[2]; dataClass->getNightTime(n);
            hour = buildHour(night[0], night[1]);
            String prevHourOff = buildHour(n[0], n[1]);
            dataClass->setNightTime(night[0], night[1]);
            waitBox(true);
            _setStringIfChanged(safeEmail + "/InsertedData/Light/HourOff", hour, prevHourOff);
            dataClass->saveToPrefs();
            waitBox(false);
        }
    };

    if (btn[2]->read()) {
        confirmLight(selection);
        if (selection != 3) selection++;
        else { selection = 0; *menu = -1; }
    }
    if (btn[3]->read()) {
        confirmLight(selection);
        if (selection != 0) selection--;
        else { selection = 0; *menu = -1; }
    }

    int data[4] = {day[0], day[1], night[0], night[1]};
    String fd[4];
    for (int i = 0; i < 4; i++)
        fd[i] = data[i] < 10 ? "0" + String(data[i]) : String(data[i]);

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);
    display->drawString(fd[0], 224,  85, 8);
    display->drawString(":",   296,  75, 8);
    display->drawString(fd[1], 374,  85, 8);
    display->drawString(fd[2], 224, 200, 8);
    display->drawString(":",   296, 190, 8);
    display->drawString(fd[3], 374, 200, 8);
    display->setTextDatum(TL_DATUM);

    boxButton(50,  48,  108, 108, YELLOW, sunIcon,  false, "Relogio", "DIA",   2);
    boxButton(170, 130, 108,  26, YELLOW, NULL, selection==0, "", "- HORA +",  2);
    boxButton(320, 130, 108,  26, YELLOW, NULL, selection==1, "", "- MIN +",   2);
    boxButton(50,  163, 108, 108, BLUE,   moonIcon, false, "",      "NOITE",   2);
    boxButton(170, 245, 108,  26, BLUE,   NULL, selection==2, "", "- HORA +",  2);
    boxButton(320, 245, 108,  26, BLUE,   NULL, selection==3, "", "- MIN +",   2);
}

void Display::tempMenu(int *menu)
{
    static int selection = 0;
    static float temp_value = dataClass->getTargetTemp();
    static float tol_value  = dataClass->getTempTolerance();
    if (selection < 0) selection = 0;
    if (selection > 1) selection = 1;

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Temperatura");
    botScreen(labels);

    auto clamp = [](float &v, float lo, float hi) {
        if (v > hi) v = lo;
        if (v < lo) v = hi;
    };

    if (btn[0]->read() || btn[0]->held()) {
        if (selection == 0) { temp_value -= 0.5f; clamp(temp_value, 0, 60); }
        if (selection == 1) { tol_value  -= 0.5f; clamp(tol_value,  1, 10); }
    }
    if (btn[1]->read() || btn[1]->held()) {
        if (selection == 0) { temp_value += 0.5f; clamp(temp_value, 0, 60); }
        if (selection == 1) { tol_value  += 0.5f; clamp(tol_value,  1, 10); }
    }

    auto confirmTemp = [&](int sel) {
        if (sel == 0) {
            float prev_tt = dataClass->getTargetTemp();
            dataClass->setTargetTemp(temp_value);
            sendPacket(TT, temp_value);
            waitBox(true);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp", temp_value, prev_tt);
            dataClass->saveToPrefs();
            waitBox(false);
        }
        if (sel == 1) {
            float prev_ttol = dataClass->getTempTolerance();
            dataClass->setTempTolerance(tol_value);
            sendPacket(TTOL, tol_value);
            waitBox(true);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", tol_value, prev_ttol);
            dataClass->saveToPrefs();
            waitBox(false);
        }
    };

    if (btn[2]->read()) {
        confirmTemp(selection);
        if (selection != 1) selection++;
        else { selection = 0; *menu = -1; }
    }
    if (btn[3]->read()) {
        confirmTemp(selection);
        if (selection != 0) selection--;
        else { selection = 0; *menu = -1; }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);
    display->drawString(temp_value < 10 ? " "+String(temp_value,1)+" " : String(temp_value,1), 320,  85, 8);
    display->drawString(tol_value  < 10 ? " "+String(tol_value, 1)+" " : String(tol_value, 1), 320, 200, 8);
    display->setTextDatum(TL_DATUM);

    boxButton(50,  48, 108, 108, RED,    tempIcon,      false, "",   "TEMPERATURA", 2);
    boxButton(270, 130, 108, 26, RED,    NULL, selection==0,   "", "-  .C  +",      2);
    boxButton(50, 163, 108, 108, ORANGE, toleranceIcon, false, "",   "TOLERANCIA",  2);
    boxButton(270, 245, 108, 26, ORANGE, NULL, selection==1,   "", "-  .C  +",      2);
}

void Display::humidMenu(int *menu)
{
    static int selection = 0;
    static float humid_value = dataClass->getTargetHumid();
    static float tol_value   = dataClass->getHumidTolerance();
    if (selection < 0) selection = 0;
    if (selection > 1) selection = 1;

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Umidade Relativa");
    botScreen(labels);

    auto clamp = [](float &v, float lo, float hi) {
        if (v > hi) v = lo; if (v < lo) v = hi;
    };

    if (btn[0]->read() || btn[0]->held()) {
        if (selection == 0) { humid_value -= 0.5f; clamp(humid_value, 0, 100); }
        if (selection == 1) { tol_value   -= 0.5f; clamp(tol_value,   1,  10); }
    }
    if (btn[1]->read() || btn[1]->held()) {
        if (selection == 0) { humid_value += 0.5f; clamp(humid_value, 0, 100); }
        if (selection == 1) { tol_value   += 0.5f; clamp(tol_value,   1,  10); }
    }

    auto confirmHumid = [&](int sel) {
        if (sel == 0) {
            float prev_th = dataClass->getTargetHumid();
            dataClass->setTargetHumid(humid_value);
            sendPacket(TH, humid_value);
            waitBox(true);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid", humid_value, prev_th);
            dataClass->saveToPrefs();
            waitBox(false);
        }
        if (sel == 1) {
            float prev_htol = dataClass->getHumidTolerance();
            dataClass->setHumidTolerance(tol_value);
            sendPacket(HTOL, tol_value);
            waitBox(true);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance", tol_value, prev_htol);
            dataClass->saveToPrefs();
            waitBox(false);
        }
    };

    if (btn[2]->read()) {
        confirmHumid(selection);
        if (selection != 1) selection++;
        else { selection = 0; *menu = -1; }
    }
    if (btn[3]->read()) {
        confirmHumid(selection);
        if (selection != 0) selection--;
        else { selection = 0; *menu = -1; }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);
    display->drawString(humid_value < 10 ? " "+String(humid_value,1)+" " : String(humid_value,1), 320,  85, 8);
    display->drawString(tol_value   < 10 ? " "+String(tol_value,  1)+" " : String(tol_value,  1), 320, 200, 8);
    display->setTextDatum(TL_DATUM);

    boxButton(50,   48, 108, 108, PURPLE, humidIcon,     false, "", "UMIDADE",    2);
    boxButton(270, 130, 108,  26, PURPLE, NULL, selection==0,   "", "-  %  +",    2);
    boxButton(50,  163, 108, 108, BLUE,   toleranceIcon, false, "", "TOLERANCIA", 2);
    boxButton(270, 245, 108,  26, BLUE,   NULL, selection==1,   "", "-  %  +",    2);
}

void Display::soilMenuSensor(int *menu)
{
    static int selection = 0;
    if (dataClass->getTargetSoil() > 100) dataClass->setTargetSoil(100);

    static float soil_value   = dataClass->getTargetSoil();
    static float pump_duration = dataClass->getPumpDuration();
    static float abs_delay    = dataClass->getAbsorptionDelay();
    static float soil_tol     = dataClass->getSoilTolerance();

    if (selection < 0) selection = 0;
    if (selection > 3) selection = 3;

    auto clamp = [](float &v, float lo, float hi) {
        if (v > hi) v = lo; if (v < lo) v = hi;
    };
    clamp(soil_value,   0, 100);
    clamp(pump_duration, 0, 300);
    clamp(abs_delay,     0, 300);
    clamp(soil_tol,      0,  10);

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Rega e Bomba");
    botScreen(labels);

    if (btn[0]->read() || btn[0]->held()) {
        if (selection == 0) soil_value   -= 0.5f;
        if (selection == 1) pump_duration -= 1.0f;
        if (selection == 2) abs_delay     -= 1.0f;
        if (selection == 3) soil_tol      -= 0.5f;
    }
    if (btn[1]->read() || btn[1]->held()) {
        if (selection == 0) soil_value   += 0.5f;
        if (selection == 1) pump_duration += 1.0f;
        if (selection == 2) abs_delay     += 1.0f;
        if (selection == 3) soil_tol      += 0.5f;
    }

    auto confirmSoil = [&](int sel) {
        waitBox(true);
        if (sel == 0) {
            float prev = dataClass->getTargetSoil();
            dataClass->setTargetSoil(soil_value);
            sendPacket(TS, soil_value);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil", soil_value, prev);
        }
        if (sel == 1) {
            float prev = dataClass->getPumpDuration();
            dataClass->setPumpDuration(pump_duration);
            sendPacket(PD, pump_duration);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", pump_duration, prev);
        }
        if (sel == 2) {
            float prev = dataClass->getAbsorptionDelay();
            dataClass->setAbsorptionDelay(abs_delay);
            sendPacket(AD, abs_delay);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", abs_delay, prev);
        }
        if (sel == 3) {
            float prev = dataClass->getSoilTolerance();
            dataClass->setSoilTolerance(soil_tol);
            sendPacket(STOL, soil_tol);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance", soil_tol, prev);
        }
        dataClass->saveToPrefs();
        waitBox(false);
    };

    if (btn[2]->read()) {
        confirmSoil(selection);
        if (selection != 3) selection++;
        else { selection = 0; *menu = -1; }
    }
    if (btn[3]->read()) {
        confirmSoil(selection);
        if (selection != 0) selection--;
        else { selection = 0; *menu = -1; }
    }

    String fd[4] = {
        (soil_value<10||soil_value==100) ? " "+String(int(soil_value))+" " : String(soil_value,1),
        pump_duration<10 ? "  "+String(int(pump_duration))+"  " : String(int(pump_duration)),
        abs_delay<10     ? "  "+String(int(abs_delay))+"  "     : String(int(abs_delay)),
        soil_tol<10      ? " "+String(soil_tol,1)+" "           : String(soil_tol,1)
    };

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);
    display->drawString(fd[0], 182,  95, 6);
    display->drawString(fd[1], 182, 210, 6);
    display->drawString(fd[2], 412,  95, 6);
    display->drawString(fd[3], 412, 210, 6);
    display->setTextDatum(TL_DATUM);

    boxButton( 16,  48, 108, 108, GREEN,  soilIcon,      false, "",
               "SOLO",       2);
    boxButton(130, 130, 108,  26, GREEN,  NULL, selection==0,   "", "-  %  +", 2);
    boxButton( 16, 163, 108, 108, BLUE,   watchIcon,     false,
               String((dataClass->getPumpFlow()*pump_duration)/10)+"mL", "DURACAO", 2);
    boxButton(130, 245, 108,  26, BLUE,   NULL, selection==1,   "", "-  S  +", 2);
    boxButton(246,  48, 108, 108, ORANGE, watchIcon,     false, "SENSOR",  "INTERVALO", 2);
    boxButton(360, 130, 108,  26, ORANGE, NULL, selection==2,   "", "-  S  +", 2);
    boxButton(246, 163, 108, 108, BROWN,  toleranceIcon, false, "",        "TOLERANCIA", 2);
    boxButton(360, 245, 108,  26, BROWN,  NULL, selection==3,   "", "-  %  +", 2);
}

void Display::soilMenuTimer(int *menu)
{
    static int selection = 0;
    float abs_delay    = dataClass->getAbsorptionDelay() / 3600.0f;
    float pump_duration = dataClass->getPumpDuration();

    if (selection < 0) selection = 1;
    if (selection > 1) selection = 0;
    if (abs_delay > 24) abs_delay = 1;
    if (abs_delay < 1)  abs_delay = 24;
    if (pump_duration > 300) pump_duration = 0;
    if (pump_duration < 0)   pump_duration = 300;

    String labels[4] = {"-", "+", "Confirm.", "Voltar"};
    topScreen("Rega e Bomba");
    botScreen(labels);

    if (btn[0]->read() || btn[0]->held()) {
        if (selection == 0) { abs_delay    -= 1; dataClass->setAbsorptionDelay(abs_delay*3600); }
        if (selection == 1) { pump_duration -= 1; dataClass->setPumpDuration(pump_duration); }
    }
    if (btn[1]->read() || btn[1]->held()) {
        if (selection == 0) { abs_delay    += 1; dataClass->setAbsorptionDelay(abs_delay*3600); }
        if (selection == 1) { pump_duration += 5; dataClass->setPumpDuration(pump_duration); }
    }
    if (btn[2]->read()) {
        waitBox(true);
        if (selection == 0) {
            dataClass->setAbsorptionDelay(abs_delay*3600);
            sendPacket(AD, dataClass->getAbsorptionDelay());
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay",
                   dataClass->getAbsorptionDelay(), dataClass->getAbsorptionDelay());
        }
        if (selection == 1) {
            dataClass->setPumpDuration(pump_duration);
            sendPacket(PD, dataClass->getPumpDuration());
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", pump_duration, dataClass->getPumpDuration());
        }
        dataClass->saveToPrefs();
        waitBox(false);
        if (selection != 1) selection++;
        else { selection = 0; *menu = -1; }
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { selection = 0; *menu = -1; }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);
    display->drawString(abs_delay<10    ? " "+String(int(abs_delay))+" "    : String(int(abs_delay)),    304,  85, 8);
    display->drawString(pump_duration<10 ? "  "+String(int(pump_duration))+"  " : String(int(pump_duration)), 304, 200, 8);
    display->setTextDatum(TL_DATUM);

    boxButton(50,  48, 108, 108, YELLOW, watchIcon,    false, "TIMER",    "INTERVALO", 2);
    boxButton(250, 130, 108, 26, YELLOW, NULL, selection==0,  "", "- HORA +", 2);
    boxButton(50, 163, 108, 108, BLUE,   wateringIcon, false,
              String(dataClass->getPumpFlow()*pump_duration/10)+"mL", "DURACAO", 2);
    boxButton(250, 245, 108, 26, BLUE, NULL, selection==1, "", "- SEGS +", 2);
}

void Display::calibrationScreen(int *menu)
{
    static int submenu      = -2;
    static int last_submenu = -2;

    if (submenu == -1) { submenu = -2; *menu = -1; }
    if (submenu > 1)    submenu = -2;

    if (submenu != last_submenu) { drawBackGround(); last_submenu = submenu; }

    switch (submenu) {
    case -2: {
        String label[4] = {"<", ">", "Selecionar", "Voltar"};
        topScreen("Menu Calibracao");
        botScreen(label);
        String labels[2][4] = {{"Sensor Reserv.","Bomba","",""},{"","","",""}};
        int colors[2][4] = {{YELLOW,RED,DARK_GREY,DARK_GREY},{DARK_GREY,DARK_GREY,DARK_GREY,DARK_GREY}};
        const uint16_t *images[2][4] = {{waterReservIcon,pumpIcon,NULL,NULL},{NULL,NULL,NULL,NULL}};
        buttonScreen(labels, colors, images, &submenu, 1);
        break;
    }
    case 0: waterReservCalibScreen(&submenu); break;
    case 1: pumpCalibScreen(&submenu); break;
    // soilSensorCalibScreen e wateringCalibScreen removidos — rega sempre por timer
    default: break;
    }
}

void Display::soilSensorCalibScreen(int *menu)
{
    static int selection = 0;
    if (selection < 0) selection = 1;
    if (selection > 1) selection = 0;

    String label[4] = {"<", ">", "Confirmar", "Voltar"};
    topScreen("Calib. Sensor do solo");
    botScreen(label);

    String soil_low = String(int(dataClass->getSoil()));
    String soil_up  = String(int(dataClass->getSoil()));

    if (btn[0]->read()) selection--;
    if (btn[1]->read()) selection++;
    if (btn[2]->read()) {
        waitBox(true);
        if (selection == 0) {
            dataClass->setSoilLow(dataClass->getSoil());
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Dry",
                                    (float)dataClass->getSoil());
            soil_low += " - OK";
        }
        if (selection == 1) {
            dataClass->setSoilUpper(dataClass->getSoil());
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/Soil/Calibration/SoilSensor/Wet",
                                    (float)dataClass->getSoil());
            soil_up += " - OK";
        }
        waitBox(false);
        if (selection != 1) selection++;
        else { selection = 0; *menu = -2; }
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { selection = 0; *menu = -2; }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Retire o sensor do solo e coloque-o, com auxilio da haste, num copo", 20, 50, 2);
    display->drawString("vazio e aguarde a leitura se estabilizar, logo apos confirme",        20, 67, 2);
    display->drawString("pressionando o botao verde.",                                         20, 84, 2);
    boxButton(80,  105, 150, 80, ORANGE, NULL, selection==0, soil_low, "Seco",  4);
    boxButton(260, 105, 150, 80, BLUE,   NULL, selection==1, soil_up,  "Umido", 4);
    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Agora, insira o sensor no solo com a umidade desejada ate a marca.", 20, 190, 2);
    display->drawString("Tambem e possivel inserir num copo com agua, aguarde a leitura se",  20, 207, 2);
    display->drawString("estabilizar e pressione o botao verde.",                             20, 224, 2);
    display->setTextColor(dataClass->isSoilCalibrated() ? GREEN : RED);
    display->drawString(dataClass->isSoilCalibrated() ? "Calibrado!" : "Nao calibrado!", 310, 240, 4);
    display->setTextColor(WHITE, DARK_GREY);
}

void Display::waterReservCalibScreen(int *menu)
{
    static int selection = 0;
    float level = dataClass->getWaterCapacity();
    if (selection < 0) selection = 1;
    if (selection > 1) selection = 0;
    if (level < 0)  level = 0;
    if (level > 20) level = 0;

    String label[4] = {selection==0?" < ":" - ", selection==0?" > ":" + ", "Confirmar", "Voltar"};
    topScreen("Calib. Sensor Reservatorio");
    botScreen(label);

    String status = dataClass->getWaterRawReading() == -1 ? "" : " - OK";
    String water  = String(dataClass->getWaterRes(), 1) + status;

    if (btn[0]->read()) {
        if (selection == 0) selection--;
        if (selection == 1) { level -= 0.1f; dataClass->setWaterCapacity(level); }
    }
    if (btn[1]->read()) {
        if (selection == 0) selection++;
        if (selection == 1) { level += 0.5f; dataClass->setWaterCapacity(level); }
    }
    if (btn[2]->read()) {
        waitBox(true);
        if (selection == 0) {
            dataClass->setWaterRawReading(dataClass->getWaterRes());
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Reserv",
                                    dataClass->getWaterRawReading());
        }
        if (selection == 1) {
            dataClass->setWaterCapacity(level);
            firebase->aSyncSetFloat(safeEmail + "/InsertedData/Sensor/WaterReserv/Calibration/Capacity",
                                    dataClass->getWaterCapacity());
        }
        waitBox(false);
        if (selection != 1) selection++;
        else { selection = 0; *menu = -2; }
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { selection = 0; *menu = -2; }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Retire a agua do recipiente, feche a tampa e aguarde a leitura se", 20, 50, 2);
    display->drawString("estabilizar, logo apos confirme pressionando o botao verde.",       20, 67, 2);
    boxButton(160,  90, 160, 65, ORANGE, NULL, selection==0, water, "Confirmar", 4);
    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Caso saiba a capacidade do seu recipiente insira abaixo apos a", 20, 155, 2);
    display->drawString("conclusao da calibracao.",                                       20, 172, 2);
    boxButton(160, 208, 160, 65, BLUE, NULL, selection==1,
              dataClass->getWaterCapacity()==NOT_DEFINED ? "Nao definido" : String(dataClass->getWaterCapacity()),
              "- Litros +", 4);
}

void Display::pumpCalibScreen(int *menu)
{
    static int   selection  = 0;
    static float flow_value = 0;
    if (selection < 0) selection = 1;
    if (selection > 1) selection = 0;
    if (flow_value < 0)    flow_value = 0;
    if (flow_value > 1000) flow_value = 1000;

    String status = String(dataClass->getPumpFlow());
    String label[4] = {selection==0?"<":" - ", selection==0?">":"+ ",
                       selection==0?"  Acionar  ":"Confirmar", "Voltar"};
    topScreen("Calib. Bomba de Agua");
    botScreen(label);

    if (btn[0]->read()) {
        if (selection == 0) selection--;
        if (selection == 1) { flow_value -= 1; dataClass->setPumpFlow(flow_value); }
    }
    if (btn[1]->read()) {
        if (selection == 0) selection++;
        if (selection == 1) { flow_value += 10; dataClass->setPumpFlow(flow_value); }
    }
    if (btn[2]->read()) {
        if (selection == 0) {
            sendPacket(PUMP_CAL, true);
            delay(10000);
            sendPacket(PUMP_CAL, false);
        }
        if (selection != 1) selection++;
        else {
            flow_value = dataClass->getPumpFlow();
            waitBox(true);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow",
                   flow_value, dataClass->getPumpFlow());
            waitBox(false);
            selection = 0; *menu = -2;
        }
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else {
            flow_value = dataClass->getPumpFlow();
            waitBox(true);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Pump/PumpFlow",
                   flow_value, dataClass->getPumpFlow());
            waitBox(false);
            selection = 0; *menu = -2;
        }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Coloque a saida da bomba ou soleinoide num recipiente graduado, aperte", 20, 50, 2);
    display->drawString("o botao e aguarde 10 segundos, depois informe o valor da leitura.",     20, 67, 2);
    boxButton(165,  90, 150, 65, ORANGE, NULL, true,
              selection==0 ? status : String(dataClass->getPumpFlow()), "- mL +", 4);
    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Outra alternativa e pesar a quantidade de agua despejada no recipiente", 20, 157, 2);
    display->drawString("e inserir seu valor da mesma forma do primeiro metodo.",                 20, 174, 2);
    boxButton(165, 203, 150, 65, BLUE, NULL, false,
              String(dataClass->getPumpFlow()/10), "mL/S", 4);
}

void Display::wateringCalibScreen(int *menu)
{
    static int selection = 0;
    if (selection < 0) selection = 1;
    if (selection > 1) selection = 0;

    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen("Ajuste Rega");
    botScreen(label);

    if (btn[0]->read()) selection--;
    if (btn[1]->read()) selection++;
    if (btn[2]->read()) {
        waitBox(true);
        if (selection == 0) {
            dataClass->setSoilBehavior(SENSOR);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Behavior", (float)SENSOR, (float)dataClass->getSoilBehavior());
            dataClass->getTargetSoil()==150 ? dataClass->setTargetSoil(50)
                                            : dataClass->setTargetSoil(dataClass->getTargetSoil());
            dataClass->setAbsorptionDelay(300);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", 300.0f, dataClass->getAbsorptionDelay());
            sendPacket(TS, dataClass->getTargetSoil());
        }
        if (selection == 1) {
            dataClass->setSoilBehavior(TIMER);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/Calibration/Behavior", (float)TIMER, (float)dataClass->getSoilBehavior());
            dataClass->setTargetSoil(150);
            dataClass->setAbsorptionDelay(3600);
            _setFloatIfChanged(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", 3600.0f, dataClass->getAbsorptionDelay());
            sendPacket(TS, dataClass->getTargetSoil());
        }
        dataClass->saveToPrefs();
        waitBox(false);
        selection = 0; *menu = -2;
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { selection = 0; *menu = -2; }
    }

    display->setTextColor(WHITE, DARK_GREY);
    display->drawString("Escolha como sua bomba de rega sera ativada. Na opcao timer a", 20, 50, 2);
    display->drawString("leitura do sensor permanece ativa mas nao influencia no",      20, 67, 2);
    display->drawString("comportamento da bomba.",                                      20, 84, 2);
    boxButton( 40, 110, 150, 150, ORANGE, NULL, selection==0,
               dataClass->getSoilBehavior()==SENSOR ? "OK" : "", "SENSOR", 4);
    boxButton(290, 110, 150, 150, BLUE,   NULL, selection==1,
               dataClass->getSoilBehavior()==TIMER  ? "OK" : "", "TIMER",  4);
}

void Display::setupScreen(int *menu)
{
    static int submenu      = -2;
    static int last_submenu = -2;

    if (submenu == -1) { submenu = -2; *menu = -1; }
    if (submenu > 3)    submenu = -2;

    if (submenu != last_submenu) { drawBackGround(); last_submenu = submenu; }

    switch (submenu) {
    case -2: {
        String label[4] = {"<", ">", "Selecionar", "Voltar"};
        topScreen("Menu Configuracao");
        botScreen(label);
        String labels[2][4] = {{"WiFi","Relogio","Atualizacoes","Sobre"},{"","","",""}};
        int colors[2][4] = {{GREEN,YELLOW,RED,ORANGE},{DARK_GREY,DARK_GREY,DARK_GREY,DARK_GREY}};
        const uint16_t *images[2][4] = {{wifiIcon,watchIcon,updateIcon,aboutIcon},{NULL,NULL,NULL,NULL}};
        buttonScreen(labels, colors, images, &submenu, 3);
        break;
    }
    case 0: wifiConfScreen(&submenu);   break;
    case 1: clockConfScreen(&submenu);  break;
    case 2: updateConfScreen(&submenu); break;
    case 3: aboutConfScreen(&submenu);  break;
    default: break;
    }
}

void Display::wifiConfScreen(int *menu)
{
    static int selection = 0;
    if (selection < 0) selection = 1;
    if (selection > 1) selection = 0;

    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen("Config. WiFi");
    botScreen(label);

    if (btn[0]->read()) selection--;
    if (btn[1]->read()) selection++;
    if (btn[2]->read()) {
        if (selection == 0) wifi->wifiInit();
        if (selection == 1) wifi->startPortal();
        if (selection != 2) selection++;
        else { selection = 0; *menu = -2; }
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { selection = 0; *menu = -2; }
    }

    String SSID = wifi->getStatus() ? wifi->getSSID() : "Clique e aguarde para conectar";
    boxButton( 40,  50, 400, 100, GREEN, NULL, selection==0, SSID,                  "CONECTAR",              4);
    boxButton( 40, 170, 400, 100, BLUE,  NULL, selection==1, "TROCAR CREDENCIAIS",  "O dispositivo ira reiniciar", 4);
}

void Display::clockConfScreen(int *menu)
{
    static int  selection = 0;
    static int  timezone  = rtc->getTimeZone() / 3600;

    if (timezone < -12) timezone = 14;
    if (timezone >  14) timezone = -12;
    if (selection < 0)  selection = 1;
    if (selection > 1)  selection = 0;

    if (selection == 0) { String l[4] = {" < "," > ","Selecionar","Voltar"}; botScreen(l); }
    if (selection == 1) { String l[4] = {" - "," + ","Confirmar","Voltar"};  botScreen(l); }
    topScreen("Config. Relogio");

    static bool   is_ok   = false;
    static String status  = "Selecione e aguarde";
    static String status2 = "Funciona apenas sem conexao";

    if (btn[0]->read()) {
        if (selection == 0) selection--;
        if (selection == 1) { timezone--; rtc->setgmtOffSet(timezone); }
    }
    if (btn[1]->read()) {
        if (selection == 0) selection++;
        if (selection == 1) { timezone++; rtc->setgmtOffSet(timezone); }
    }
    if (btn[2]->read()) {
        if (selection == 0) {
            status = wifi->getStatus() ? "Aguarde..." : "Conecte a internet!";
            if (wifi->getStatus()) is_ok = rtc->begin();
        }
        if (selection == 1) {
            if (!wifi->getStatus()) status2 = "Conecte a internet!";
            else { selection = 0; *menu = -2; }
        }
        if (selection != 2) selection++;
        else { status = "Selecione e aguarde"; selection = 0; *menu = -2; }
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { status = "Selecione e aguarde"; selection = 0; *menu = -2; }
    }

    String hour = is_ok
        ? (String(rtc->getHour()<10?"0":"")+rtc->getHour())+" : "+
          (String(rtc->getMinute()<10?"0":"")+rtc->getMinute())+" : "+
          (String(rtc->getSecond()<10?"0":"")+rtc->getSecond())
        : "";

    boxButton(40,  50, 400, 100, GREEN, NULL, selection==0, is_ok ? hour : status,  "SINCRONIZAR",  4);
    boxButton(40, 170, 400, 100, BLUE,  NULL, selection==1, String(rtc->getTimeZone())+"Hrs", "FUSO HORARIO", 4);
}

void Display::updateConfScreen(int *menu)
{
    static int selection = 0;
    if (selection < 0) selection = 1;
    if (selection > 1) selection = 0;

    String label[4] = {" "," ", selection==0?"Verificar":"Atualizar","Voltar"};
    topScreen("Config. Atualizacoes");
    botScreen(label);

    if (btn[2]->read()) {
        waitBox(true);
        ota->fetchReleaseInfo();
        waitBox(false);
        drawBackGround();
        if (selection == 1 && ota->getVersion() != ota->getReleaseTag())
            ota->updateDevice();
        if (selection != 1) selection++;
        else { selection = 0; *menu = -2; }
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { selection = 0; *menu = -2; }
    }

    boxButton(40, 50, 400, 100, GREEN, NULL, true,
              "Versao atual: " + ota->getVersion(),
              selection==0 ? " VERIFICAR " : "ATUALIZAR", 4);

    display->setTextColor(WHITE, DARK_GREY);
    if (ota->getVersion() != ota->getReleaseTag()) {
        display->drawString("Notas da nova versao: " + ota->getReleaseTag() +
                            " - " + ota->getReleaseDate(), 40, 160, 2);
        display->drawString(ota->getReleaseName(), 40, 175, 2);
        String body  = ota->getReleaseBody();
        int line = 0, start = 0, sep = body.indexOf(';');
        while (sep != -1 && line < 5) {
            String part = body.substring(start, sep+1); part.trim();
            if (part.length() > 0) display->drawString(part, 40, 190+(15*line++), 2);
            start = sep+1; sep = body.indexOf(';', start);
        }
        if (line < 5) {
            String last = body.substring(start); last.trim();
            if (last.length() > 0) display->drawString(last, 40, 190+(15*line), 2);
        }
    } else {
        display->drawString("Nao ha novas atualizacoes no momento.", 40, 160, 2);
    }
}

void Display::aboutConfScreen(int *menu)
{
    static int selection = 0;
    if (selection < 0) selection = 0;
    if (selection > 3) selection = 3;

    String label[4] = {"<", ">", "Selecionar", "Voltar"};
    topScreen("Sobre");
    botScreen(label);

    if (btn[2]->read()) {
        if (selection != 3) selection++;
        else { selection = 0; *menu = -2; }
    }
    if (btn[3]->read()) {
        if (selection != 0) selection--;
        else { selection = 0; *menu = -2; }
    }
}

void Display::connectionScreen(String label_1, String label_2)
{
    display->setTextColor(WHITE, DARK_GREY);
    display->setTextDatum(MC_DATUM);
    display->drawString(label_1, 240,  25, 4);
    display->drawString(label_2, 240, 300, 4);
}

void Display::wifiConnStatus()
{
    static unsigned long lastUpdate  = 0;
    static int           lastSignal  = 0;
    static int           smoothedRSSI = -70;

    const int initX = 20, initY = 13, barW = 5, space = 1, maxH = 14;
    unsigned long now = millis();

    if (now - lastUpdate >= 10000) {
        lastUpdate = now;
        int cur = wifi->getSignalStrenght();
        smoothedRSSI = (int)(cur * 0.7f + smoothedRSSI * 0.3f);
        if (!wifi->getStatus()) lastSignal = 0;
        else if (smoothedRSSI >= -55) lastSignal = 5;
        else if (smoothedRSSI >= -65) lastSignal = 4;
        else if (smoothedRSSI >= -75) lastSignal = 3;
        else if (smoothedRSSI >= -85) lastSignal = 2;
        else if (smoothedRSSI >= -95) lastSignal = 1;
        else lastSignal = 0;
    }

    int col = wifi->getStatus() ? WHITE : RED;
    display->fillRect(initX, initY, (barW+space)*5, maxH, BLACK);
    for (int i = 0; i < 5; i++) {
        int h = (i+1)*3;
        display->fillRect(initX + i*(barW+space), initY+(maxH-h), barW, h,
                          i < lastSignal ? col : DARK_GREY);
    }
}

void Display::qrScreen()
{
    display->setTextDatum(MC_DATUM);
    display->fillScreen(BG_ICON);
    display->setTextColor(WHITE, BG_ICON);
    display->drawBitmap(165, 50, wifiqr, 150, 150, WHITE);
    display->drawString("Para configurar sua rede e credenciais, aponte seu celular",    240, 255, 2);
    display->drawString("para este QR Code ou procure por Growbox na sua rede WiFi.",   240, 270, 2);
    display->setTextDatum(TL_DATUM);
}