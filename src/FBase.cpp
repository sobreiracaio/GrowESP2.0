#include "FBase.hpp"

void FBase::processData(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available())
    {
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
}

FBase::FBase(const String& api, const String& db_url, const String& user_email, const String& user_password)
    : apiKey(api), 
      dbUrl(db_url), 
      email(user_email), 
      password(user_password),
      user_auth(apiKey.c_str(), email.c_str(), password.c_str()),
      aClient(ssl_client),
      authenticated(false)       
      {}

bool FBase::init() {
    const unsigned long INIT_TIMEOUT = 30000; // 30 segundos
    const unsigned long LOOP_INTERVAL = 50;   // Intervalo entre loops
    
    unsigned long startTime = millis();
    unsigned long lastLoopTime = 0;
    
    ssl_client.setTimeout(5);
    
    if (ssl_client.connected()) {
        ssl_client.stop();
    }
    
    ssl_client.setInsecure();
    
    initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
    
    // Loop sem bloqueio com timeout
    while (app.isInitialized() && !app.ready()) {
        unsigned long currentTime = millis();
        
        // Timeout
        if (currentTime - startTime > INIT_TIMEOUT) {
            if (ssl_client.connected()) ssl_client.stop();
            authenticated = false;
            return false;
        }
        
        // Executa loop apenas no intervalo definido
        if (currentTime - lastLoopTime >= LOOP_INTERVAL) {
            lastLoopTime = currentTime;
            app.loop();
        }
        
        yield(); // Mantém watchdog feliz
    }
    
    if (app.ready()) {
        authenticated = true;
        app.getApp<RealtimeDatabase>(Database);
        Database.url(dbUrl.c_str());
        aClient.setSyncReadTimeout(10);
        aClient.setSyncSendTimeout(10);
        return true;
    }
    
    if (ssl_client.connected()) ssl_client.stop();
    authenticated = false;
    return false;
}

bool FBase::stopApp()
{
    if (!authenticated) return true;
    
    const unsigned long STOP_TIMEOUT = 3000; // 3 segundos
    const unsigned long CHECK_INTERVAL = 10; // Verifica a cada 10ms
    
    unsigned long startTime = millis();
    unsigned long lastCheckTime = 0;
    
    aClient.stopAsync(true);
    
    // Espera não-bloqueante com timeout
    while (ssl_client.connected()) {
        unsigned long currentTime = millis();
        
        // Timeout - força fechamento
        if (currentTime - startTime > STOP_TIMEOUT) {
            ssl_client.stop();
            break;
        }
        
        // Verifica status apenas no intervalo definido
        if (currentTime - lastCheckTime >= CHECK_INTERVAL) {
            lastCheckTime = currentTime;
            // ssl_client.connected() já é verificado no while
        }
        
        yield(); // Mantém watchdog feliz
    }
    
    authenticated = false;
    return true;
}

void FBase::loop() {
    if (!authenticated || !app.ready()) return;
    
    // ✅ Verifica se SSL ainda está vivo
    if (!ssl_client.connected() && WiFi.status() != WL_CONNECTED) {
        authenticated = false;
        return;
    }
    
    ssl_client.setTimeout(5); // Timeout agressivo
    app.loop();
    Database.loop();
}

bool FBase::isHealthy() 
{
    return authenticated && 
           app.ready() && 
           (ssl_client.connected() || WiFi.status() == WL_CONNECTED);
}

bool FBase::isReady() {
    return authenticated && app.ready();
}

String* FBase::asyncTarget = nullptr;

void FBase::asyncCallback(AsyncResult &aResult)
{
    if (!asyncTarget) return;
    if (aResult.isResult() && aResult.available())
        *asyncTarget = aResult.c_str();
}

void FBase::aSyncGet(String& path, String &result)
{
    if (!isReady()) return;
    asyncTarget = &result;
    Database.get(aClient, path.c_str(), asyncCallback, false, "aSyncGetTask");
}

static bool _pendingString = false;

void FBase::aSyncSetString(String &path, String &value)
{
    if (!isReady() || _pendingString) return;

    _pendingString = true;

    Database.set(aClient, path, value.c_str(), [](AsyncResult &result) {
        _pendingString = false;
        if (result.isError())
            Serial.printf("[Firebase] Erro string: %s\n", result.error().message().c_str());
    });

    unsigned long timeout = millis();
    unsigned long lastYield = millis();

    while (_pendingString && millis() - timeout < 3000) {
        loop();
        esp_task_wdt_reset();
        if (millis() - lastYield > 10) {
            lastYield = millis();
            yield();
        }
    }

    if (_pendingString) {
        Serial.println("[Firebase] timeout string");
        _pendingString = false;
    }
}

static bool _pendingFloat = false;

void FBase::aSyncSetFloat(String &path, float &value)
{
    if (!isReady() || _pendingFloat) return;

    _pendingFloat = true;

    Database.set(aClient, path, value, [](AsyncResult &result) {
        _pendingFloat = false;
        if (result.isError())
            Serial.printf("[Firebase] Erro float: %s\n", result.error().message().c_str());
    });

    unsigned long timeout = millis();
    unsigned long lastYield = millis();

    while (_pendingFloat && millis() - timeout < 3000) {
        loop();
        esp_task_wdt_reset();
        if (millis() - lastYield > 10) {
            lastYield = millis();
            yield();
        }
    }

    if (_pendingFloat) {
        Serial.println("[Firebase] timeout float");
        _pendingFloat = false;
    }
}

static bool _pendingBool = false;

void FBase::aSyncSetBool(String &path, bool &value)
{
    if (!isReady() || _pendingBool) return;

    _pendingBool = true;

    Database.set(aClient, path, value, [](AsyncResult &result) {
        _pendingBool = false;
        if (result.isError())
            Serial.printf("[Firebase] Erro bool: %s\n", result.error().message().c_str());
    });

    unsigned long timeout = millis();
    unsigned long lastYield = millis();

    while (_pendingBool && millis() - timeout < 3000) {
        loop();
        esp_task_wdt_reset();
        if (millis() - lastYield > 10) {
            lastYield = millis();
            yield();
        }
    }

    if (_pendingBool) {
        Serial.println("[Firebase] timeout bool");
        _pendingBool = false;
    }
}

void FBase::awaitGet(String& path, String *result)
{
    if (!isReady()) return;
    
    // ✅ TIMEOUT POR TENTATIVA
    const unsigned long GET_TIMEOUT = 5000; // 5 segundos
    int retries = 2; // Reduzido de 3 para 2
    
    while (retries > 0) {
        unsigned long startTime = millis();
        
        *result = Database.get<String>(aClient, path.c_str());
        
        // ✅ Verifica timeout
        if (millis() - startTime > GET_TIMEOUT) {
            retries--;
            continue;
        }
        
        if (result->length() > 0 || aClient.lastError().code() == 0) {
            break;
        }
        
        retries--;
        
        // ✅ Delay não-bloqueante
        unsigned long delayStart = millis();
        while (millis() - delayStart < 300) {
            yield();
        }
    }
}

void FBase::awaitSet(String &path, String value, int type)
{
    if (!isReady()) return;
    
    // ✅ TIMEOUT POR TENTATIVA
    const unsigned long SET_TIMEOUT = 5000;
    int retries = 2;
    bool success = false;
    
    while (retries > 0 && !success) {
        unsigned long startTime = millis();
        
        if(type == INT)
            success = Database.set<int>(aClient, path.c_str(), value.toInt());
        else if(type == FLOAT)
            success = Database.set<float>(aClient, path.c_str(), value.toFloat());
        else if(type == STRING)
            success = Database.set<String>(aClient, path.c_str(), value.c_str());
        else if(type == BOOL)
            success = Database.set<bool>(aClient, path.c_str(), value.toInt());
        
        // ✅ Timeout forçado
        if (millis() - startTime > SET_TIMEOUT) {
            retries--;
            continue;
        }
        
        if (!success) {
            retries--;
            unsigned long delayStart = millis();
            while (millis() - delayStart < 300) {
                yield();
            }
        }
    }
}