#include "WifiManager.hpp"

WifiManager::WifiManager(Preferences *preferences, Display *tft) : prefs(preferences), display(tft) 
{}

void WifiManager::injectDisplay(Display *tft)
{
    display = tft;
}

void WifiManager::ensureCredentialsLoaded() 
{
    if (credentialsLoaded) return;
    
    prefs->begin("wifi", false);
    ssid     = prefs->getString("ssid",     "");
    password = prefs->getString("password", "");
    email    = prefs->getString("email",    "");
    userpass = prefs->getString("userpass", "");
    prefs->end();
    
    credentialsLoaded = true;
}

bool WifiManager::wifiInit()
{
    ensureCredentialsLoaded();

    if (ssid.isEmpty() || password.isEmpty() || email.isEmpty() || userpass.isEmpty()) 
    {
        delay(2000);
        startPortal();
        return false;
    }

    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    // Tenta até 5 vezes antes de abrir portal
    // Cobre roteador lento após queda de energia (15s por tentativa, 3s entre elas = ~90s total)
    const int          MAX_ATTEMPTS    = 5;
    const unsigned long ATTEMPT_TIMEOUT = 15000;
    const unsigned long BETWEEN_DELAY   = 3000;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        Serial.printf("[WiFi] Tentativa %d/%d conectando a: %s\n", attempt, MAX_ATTEMPTS, ssid.c_str());

        WiFi.disconnect(true);
        delay(200);
        WiFi.begin(ssid.c_str(), password.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < ATTEMPT_TIMEOUT) {
            esp_task_wdt_reset();
            yield();
        }

        if (WiFi.status() == WL_CONNECTED) {
            // Aguarda DHCP atribuir IP válido (evita Firebase conectar com 0.0.0.0)
            unsigned long ipWait = millis();
            while (WiFi.localIP() == IPAddress(0,0,0,0) && millis() - ipWait < 5000) {
                esp_task_wdt_reset();
                yield();
            }
            if (WiFi.localIP() == IPAddress(0,0,0,0)) {
                Serial.println("[WiFi] DHCP nao respondeu, tentando novamente...");
                continue; // trata como falha e tenta de novo
            }
            Serial.printf("[WiFi] Conectado na tentativa %d! IP: %s\n", attempt, WiFi.localIP().toString().c_str());
            return true;
        }

        Serial.printf("[WiFi] Tentativa %d falhou (status %d).\n", attempt, WiFi.status());

        if (attempt < MAX_ATTEMPTS) {
            unsigned long wait = millis();
            while (millis() - wait < BETWEEN_DELAY) {
                esp_task_wdt_reset();
                yield();
            }
        }
    }

    // Só abre portal depois de esgotar todas as tentativas
    Serial.println("[WiFi] Todas as tentativas falharam. Abrindo portal de configuracao.");
    startPortal();
    return false;
}

bool WifiManager::checkCredentials()
{
    if (ssid.isEmpty() || password.isEmpty() || email.isEmpty() || userpass.isEmpty()) 
        return false;
    return true;
}

void WifiManager::startPortal() 
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP("GrowBox-Setup", "12345678");
    IPAddress IP = WiFi.softAPIP();
    
    display->qrScreen();
    dnsServer.start(53, "*", IP);

    webServer.on("/", std::bind(&WifiManager::handleRoot, this));
    webServer.on("/save", std::bind(&WifiManager::handleSave, this));

    webServer.onNotFound([this, IP]() 
    {
        webServer.sendHeader("Location", String("http://") + IP.toString(), true);
        webServer.send(302, "text/plain", "");
    });

    webServer.begin();

    unsigned long previousMillis = 0;
    const unsigned long interval = 10;

    while (true) 
    {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) 
        {
            previousMillis = currentMillis;
            dnsServer.processNextRequest();
            webServer.handleClient();
        }
    }
}

void WifiManager::handleRoot() 
{
    String img = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAACXBIWXMAAC4jAAAuIwF4pT92AAADmUlEQVR4AeyUX0xbVRzHP/1HBy0yCgXKMIIgxojJ4oMPyhIxJiYkzAUSmHuQzAd9mizGJ3yYWYzb4oOoD0YTfQAfTMicaOKfBxMzEpVkqFEzZMAY8balhd7Wld6297b3em4Ny16WnFu27GW3PfeenP7O7/vJ93tu3dZdvtzc5esewJ4cKJVKew6waoDl5WV6enuYnZ3dE0RVALb4seNHGJ3q5YPPTu8JwjFAoVDg6IuHGT/dQ2ubn5NnD/PhzFssLCxU5YQjANM0eeHYUZ4b7+SJh57l/sABtnWVsRMHefX1l4lGo44hHAFMT09jhdd47KkmTKuMyzJJ5rfo6uzgmfFGJt+YvHMAxWKR9z46S/9LnWjlPCvpn4WYSY3LxI2Hhx8PkTAuOY5C2oGpqSm6nq5F1Q1Wczt4PQHagr3othNuv3iaPHIkzKk3Twkw+a8UgJ395xc+5eBQhJ5gHQ2+GpaycVbVX/ALB3SzREwcTiPspbB/ncXFRWkCKYD5+Xn2dfuIaSXiIgruGiWMQycmMRXcLhcKUqUyWC4eeLKRubm52wugKApme4jL1zpZTiEc8BLyCaCijiU+2WJSHEqrItrSG0RR5N8GKQe++nqOhgdbiTSpKMk2llTYLBbIm2Uyuk6mKKgEiE1Q9Fn8uTpvT6WGFAC4CDa6aNqfoSGosRFvxyh7QKynCh4yBQPEXCSAZvjw1km2BaQqD/UfInopweWrXeglL4HaIgk1JLZDOleHVvSLCFxsqvehRBuI1D5a+U3mJgXQ0dHB9hUNU0lS0t0YhpekADDKLuGEqyK+EW9G2WznekznQEe7jHalRgpgaGiInZVtajNbmH+tk/3XR7nsJZsLkN0JkMr7SGWaKw3Tv19leHi4Mpe5SQF4PB6OPz9GKp7Hm9eoV/4GyyKdraeguflDCeLLpfFvXKE5HWBgYEBGu1IjBWBXTkxMEP1ti7KnBq+Ww69G0VQTbyzOPmWN4D/LRH+6xolXTtrl0kMaIBKJ8Mm7H7N0MSkiMKndjmHEc3gSCVBVVn7NMNY/zOjoqLS4XSgNYBcPDg7y7cyXtGqdrF2MkfxugXXxdphr9bzz2jnePnPGLnM0HAHYnfv6+jh//gsuTH8jomjg3OT7/Pj9D44Ont1ndzgGuLHR7aa7u5uRkZHdpaqeVQO0tLQwMzNTlejNm6oGCIfDhEL//xve3NDpvGoAp0K3qr8HcMcduJX1u+v/AQAA//92/UEGAAAABklEQVQDAKmv5dDx//LBAAAAAElFTkSuQmCC";

    String html = "<!DOCTYPE html><html lang='pt-br'><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Configuração WiFi</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; display:flex; flex-direction:column; align-items:center; justify-content:center; text-align:center; margin:0; padding:20px; background:#121212; color:#e0e0e0; }";
    html += "h1 { color:#ffffff; }";
    html += "form { background:#1e1e1e; padding:20px; border-radius:10px; box-shadow:0 0 10px rgba(0,0,0,0.5); width:100%; max-width:350px; }";
    html += "input, select { width:90%; padding:10px; margin:10px 0; border-radius:5px; border:1px solid #555; background:#333; color:#fff; }";
    html += "input[type='submit'] { background:#4CAF50; color:white; border:none; cursor:pointer; }";
    html += "input[type='submit']:hover { background:#45a049; }";
    html += "img.logo { width:80px; margin-bottom:15px; }";
    html += ".show-pass { display:flex; align-items:center; justify-content:flex-start; font-size:0.9em; }";
    html += ".show-pass input { width:auto; margin-left:10px; }";
    html += "</style>";
    html += "<script>";
    html += "function togglePassword(id) {";
    html += "  var x = document.getElementById(id);";
    html += "  if(x.type === 'password') x.type='text'; else x.type='password';";
    html += "}";
    html += "</script>";
    html += "</head><body>";

    html += "<img class='logo' src='data:image/png;base64,"+ img +"' alt='Logo'>";
    html += "<h1>Configurar WiFi</h1>";
    html += "<form action='/save' method='post'>";
    html += "Rede WiFi:<br>";
    html += "<select name='ssid'>" + generateNetWorkList() + "</select><br>";
    html += "Senha WiFi:<br><input type='password' id='wifi_pass' name='password'><div class='show-pass'><label>Mostrar senha</label><input type='checkbox' onclick='togglePassword(\"wifi_pass\")'></div>";
    html += "E-mail:<br><input type='email' name='email'><br>";
    html += "Senha de Acesso:<br><input type='password' id='user_pass' name='userpass'><div class='show-pass'><label>Mostrar senha</label><input type='checkbox' onclick='togglePassword(\"user_pass\")'></div>";
    html += "<input type='submit' value='Salvar'>";
    html += "</form></body></html>";

    webServer.send(200, "text/html", html);
}

void WifiManager::handleSave() 
{
    if (webServer.hasArg("ssid") && webServer.hasArg("password") && 
        webServer.hasArg("email") && webServer.hasArg("userpass")) 
    {
        String newSsid     = webServer.arg("ssid");
        String newPass     = webServer.arg("password");
        String newEmail    = webServer.arg("email");
        String newUserPass = webServer.arg("userpass");

        prefs->begin("wifi", false);
        prefs->putString("ssid",     newSsid);
        prefs->putString("password", newPass);
        prefs->putString("email",    newEmail);
        prefs->putString("userpass", newUserPass);
        prefs->end();

        webServer.send(200, "text/html", 
            "<html><body><h1>Salvo!</h1><p>O dispositivo vai reiniciar.</p></body></html>");
        delay(2000);
        ESP.restart();
    } 
    else 
    {
        webServer.send(400, "text/plain", "Faltam parametros.");
    }
}

String WifiManager::generateNetWorkList() 
{
    String options = "";
    int n = WiFi.scanNetworks();
    if (n == 0) 
    {
        options = "<option value=''>Nenhuma rede encontrada</option>";
    } 
    else 
    {
        for (int i = 0; i < n; i++) 
        {
            options += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + 
                       " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
        }
    }
    WiFi.scanDelete();
    return options;
}

void WifiManager::handleReset() 
{
    prefs->begin("wifi", false);
    prefs->clear();
    prefs->end();
    delay(2000);
    ESP.restart();
}

String WifiManager::getEmail()
{
    ensureCredentialsLoaded();
    return email;
}

String WifiManager::getPass()
{
    ensureCredentialsLoaded();
    return userpass;
}

// ── loop — não-bloqueante ─────────────────────────────────────────────────────
// Verifica WiFi a cada 10s. Se caído, inicia reconexão e retorna imediatamente.
// Na próxima chamada verifica se reconectou ou se o timeout de 10s expirou.
void WifiManager::loop() 
{
    static unsigned long lastCheck = 0;

    // Se está no meio de uma tentativa de reconexão
    if (_reconnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            // Verifica se DHCP já atribuiu IP — pode demorar alguns segundos
            if (WiFi.localIP() == IPAddress(0,0,0,0)) return; // ainda aguardando
            _reconnecting = false;
            Serial.printf("[WiFi] Reconectado! IP: %s\n", WiFi.localIP().toString().c_str());
        } else if (millis() - _reconStart >= 10000) {
            // Timeout — desiste dessa tentativa, vai tentar de novo na próxima janela
            _reconnecting = false;
            lastCheck     = millis();
            Serial.println("[WiFi] Timeout ao reconectar.");
        }
        // Ainda aguardando — retorna sem bloquear
        return;
    }

    // Verifica a cada 10s se ainda está conectado
    if (millis() - lastCheck < 10000) return;
    lastCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
    }
}

// ── connectToWiFi — inicia reconexão e retorna imediatamente ─────────────────
// O acompanhamento do resultado fica em loop() via _reconnecting + _reconStart.
void WifiManager::connectToWiFi() 
{
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    _reconnecting = true;
    _reconStart   = millis();
    Serial.println("[WiFi] Tentando reconectar...");
}

bool WifiManager::getStatus()
{
    return WiFi.status() == WL_CONNECTED;
}

int WifiManager::getSignalStrenght()
{
    return WiFi.RSSI();
}

String WifiManager::getSSID()
{
    ensureCredentialsLoaded();
    return ssid;
}

String WifiManager::getLocalIP()
{
    return WiFi.localIP().toString();
}