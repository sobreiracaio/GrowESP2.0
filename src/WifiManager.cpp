#include "WifiManager.hpp"

WifiManager::WifiManager(Preferences *preferences, Display *tft) : prefs(preferences), display(tft) 
{}

void WifiManager::injectDisplay(Display *tft)
{
    display = tft;
}


bool WifiManager::wifiInit()
{
    prefs->begin("wifi", false);
    ssid = prefs->getString("ssid", "");
    password = prefs->getString("password", "");
    email = prefs->getString("email", "");
    userpass = prefs->getString("userpass","");

    if (ssid.isEmpty() || password.isEmpty() || email.isEmpty() || userpass.isEmpty()) 
    {
        display->connectionScreen("Credenciais incompletas!","Iniciando portal...");
        delay(2000);
        startPortal();
        return false;
    }
    wifiClient.setTimeout(2);

    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    WiFi.begin(ssid.c_str(), password.c_str());
    
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) 
    {
        display->connectionScreen("Conectando em:",ssid.c_str());
    }

    if (WiFi.status() == WL_CONNECTED) 
    {
        display->flushScreen();
        display->connectionScreen("Conectado em:",ssid.c_str());
        //Serial.printf("\n✅ Conectado em %s | IP: %s\n", ssid.c_str(), WiFi.localIP().toString().c_str());
        return true;
    } 
    else 
    {
        display->flushScreen();
        display->connectionScreen("Falha na conexão.", "Iniciando portal...");
        startPortal();
        return false;
    }

    return false;
}

void WifiManager::startPortal() 
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP("GrowBox-Setup", "12345678");
    IPAddress IP = WiFi.softAPIP();
    //Serial.print("🌐 Portal ativo em: ");
    //Serial.println(IP);
    
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
    const unsigned long interval = 10;  // 10 ms

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
    String img = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAACXBIWXMAAC4jAAAuIwF4pT92AAADmUlEQVR4AeyUX0xbVRzHP/1HBy0yCgXKMIIgxojJ4oMPyhIxJiYkzAUSmHuQzAd9mizGJ3yYWYzb4oOoD0YTfQAfTMicaOKfBxMzEpVkqFEzZMAY8balhd7Wld6297b3em4Ny16WnFu27GW3PfeenP7O7/vJ93tu3dZdvtzc5esewJ4cKJVKew6waoDl5WV6enuYnZ3dE0RVALb4seNHGJ3q5YPPTu8JwjFAoVDg6IuHGT/dQ2ubn5NnD/PhzFssLCxU5YQjANM0eeHYUZ4b7+SJh57l/sABtnWVsRMHefX1l4lGo44hHAFMT09jhdd47KkmTKuMyzJJ5rfo6uzgmfFGJt+YvHMAxWKR9z46S/9LnWjlPCvpn4WYSY3LxI2Hhx8PkTAuOY5C2oGpqSm6nq5F1Q1Wczt4PQHagr3othNuv3iaPHIkzKk3Twkw+a8UgJ395xc+5eBQhJ5gHQ2+GpaycVbVX/ALB3SzREwcTiPspbB/ncXFRWkCKYD5+Xn2dfuIaSXiIorrRknEYJIxdNwuF2UsUqUyWC4eeLKRubm52wugKApme4jL1zpZTiEc8BLyCaCijiU+2WJSHEqrItrSG0RR5N8GKQe++nqOhgdbiTSpKMk2llTYLBbIm2Uyuk6mKKgEiE1Q9Fn8uTpvT6WGFAC4CDa6aNqfoSGosRFvxyh7QKynCh4yBQPEXCSAZvjw1km2BaQqD/UfInopweWrXeglL4HaIgk1JLZDOleHVvSLCFxsqvehRBuI1D5a+U3mJgXQ0dHB9hUNU0lS0t0YhpekADDKLuGEqyK+EW9G2WznekznQEe7jHalRgpgaGiInZVtajNbmH+tk/3XR7nsJZsLkN0JkMr7SGWaKw3Tv19leHi4Mpe5SQF4PB6OPz9GKp7Hm9eoV/4GyyKdraeguflDCeLLpfFvXKE5HWBgYEBGu1IjBWBXTkxMEP1ti7KnBq+Ww69G0VQTbyzOPmWN4D/LRH+6xolXTtrl0kMaIBKJ8Mm7H7N0MSkiMKndjmHEc3gSCVBVVn7NMNY/zOjoqLS4XSgNYBcPDg7y7cyXtGqdrF2MkfxugXXxdphr9bzz2jnePnPGLnM0HAHYnfv6+jh//gsuTH8jomjg3OT7/Pj9D44Ont1ndzgGuLHR7aa7u5uRkZHdpaqeVQO0tLQwMzNTlejNm6oGCIfDhEL//xve3NDpvGoAp0K3qr8HcMcduJX1u+v/AQAA//92/UEGAAAABklEQVQDAKmv5dDx//LBAAAAAElFTkSuQmCC";

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
    if (webServer.hasArg("ssid") && webServer.hasArg("password") && webServer.hasArg("email") && webServer.hasArg("userpass")) 
    {
        String newSsid = webServer.arg("ssid");
        String newPass = webServer.arg("password");
        String newEmail = webServer.arg("email");
        String newUserPass = webServer.arg("userpass");

        prefs->putString("ssid", newSsid);
        prefs->putString("password", newPass);
        prefs->putString("email", newEmail);
        prefs->putString("userpass", newUserPass);

        webServer.send(200, "text/html", "<html><body><h1>Salvo!</h1><p>O dispositivo vai reiniciar.</p></body></html>");
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
            options += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
        }
    }
    WiFi.scanDelete();
    return options;
}

void WifiManager::handleReset() 
{
    prefs->clear();
    delay(2000);
    ESP.restart();
}

String WifiManager::getEmail()
{
    return email;
}
String WifiManager::getPass()
{
    return userpass;
}


void WifiManager::loop() 
{
    // Monitora conexão e tenta reconectar
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 10000) {
        lastCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            //Serial.println("⚠️ Conexão perdida. Tentando reconectar...");
            connectToWiFi();
        }
    }
    wifiClient.setTimeout(2);
}

void WifiManager::connectToWiFi() 
{
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long start = millis();
    unsigned long lastPrint = millis();
    const unsigned long timeout = 10000; // 10 segundos
    const unsigned long printInterval = 500; // intervalo de print

    while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) 
    {
        if (millis() - lastPrint >= printInterval) 
        {
            //Serial.print(".");
            lastPrint = millis();
        }

        // Aqui você pode colocar outras tarefas não-bloqueantes
    }

    //Serial.println(WiFi.status() == WL_CONNECTED ? "\n✅ Reconectado!" : "\n❌ Falha ao reconectar.");
}

 bool WifiManager::getStatus()
 {
    if(WiFi.status() != WL_CONNECTED)
        return false;
    else
        return true;
 }


 int WifiManager::getSignalStrenght()
 {
    return WiFi.RSSI();
 }