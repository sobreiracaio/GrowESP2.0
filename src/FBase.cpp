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

        // Detecta erro de credencial: INVALID_PASSWORD / EMAIL_NOT_FOUND / INVALID_LOGIN_CREDENTIALS
        int errCode = aResult.error().code();
        const char* errMsg = aResult.error().message().c_str();
        if (errCode == 400 ||
            strstr(errMsg, "INVALID_PASSWORD")         != NULL ||
            strstr(errMsg, "EMAIL_NOT_FOUND")          != NULL ||
            strstr(errMsg, "INVALID_LOGIN_CREDENTIALS")!= NULL ||
            strstr(errMsg, "invalid_grant")            != NULL)
        {
            if (_instance) _instance->_credentialError = true;
            Serial.println("[Firebase] Erro de credenciais detectado!");
        }
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
      {
          _instance = this; // necessário para processData acessar _credentialError
      }

// ── safeStop ─────────────────────────────────────────────────────────────────
// safeStop() pode bloquear por dezenas de segundos quando o servidor
// fechou a conexao abruptamente (MBEDTLS_ERR_NET_RECV_FAILED -76).
// Este helper aguarda o close() em loop dando wdt_reset para evitar watchdog.
void FBase::safeStop()
{
    if (!ssl_client.connected()) return;

    ssl_client.stop();  // dispara o close() — pode bloquear internamente

    // Aguarda desconexao confirmada alimentando o watchdog (max 3s)
    const unsigned long t = millis();
    while (ssl_client.connected() && millis() - t < 3000) {
        esp_task_wdt_reset();
        yield();
    }
}

bool FBase::init() {
    const unsigned long INIT_TIMEOUT = 30000;
    const unsigned long LOOP_INTERVAL = 50;
    
    unsigned long startTime = millis();
    unsigned long lastLoopTime = 0;
    
    if (ssl_client.connected()) {
        safeStop();
    }
    
    ssl_client.setInsecure();
    ssl_client.setTimeout(15000);
    
    initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
    
    while (app.isInitialized() && !app.ready()) {
        unsigned long currentTime = millis();
        
        if (currentTime - startTime > INIT_TIMEOUT) {
            if (ssl_client.connected()) safeStop();
            authenticated = false;
            return false;
        }
        
        if (currentTime - lastLoopTime >= LOOP_INTERVAL) {
            lastLoopTime = currentTime;
            app.loop();
        }
        
        esp_task_wdt_reset();
        yield();
    }
    
    if (app.ready()) {
        authenticated = true;
        app.getApp<RealtimeDatabase>(Database);
        Database.url(dbUrl.c_str());
        aClient.setSyncReadTimeout(10);
        aClient.setSyncSendTimeout(10);
        return true;
    }
    
    if (ssl_client.connected()) safeStop();
    authenticated = false;
    return false;
}

bool FBase::stopApp()
{
    if (!authenticated) return true;

    // Drena operações pendentes antes de fechar para evitar pbuf crash
    unsigned long drainStart = millis();
    while (isBusy() && millis() - drainStart < 500) {
        app.loop();
        esp_task_wdt_reset();
        yield();
    }

    // Força cancelamento se ainda pendente após drain
    _pendingFloat  = false;
    _pendingBool   = false;
    _pendingString = false;
    _lastBoolSet   = false;
    
    const unsigned long STOP_TIMEOUT = 3000;
    const unsigned long CHECK_INTERVAL = 10;
    
    unsigned long startTime = millis();
    unsigned long lastCheckTime = 0;
    
    aClient.stopAsync(true);
    
    while (ssl_client.connected()) {
        unsigned long currentTime = millis();
        
        if (currentTime - startTime > STOP_TIMEOUT) {
            safeStop();
            break;
        }
        
        if (currentTime - lastCheckTime >= CHECK_INTERVAL) {
            lastCheckTime = currentTime;
        }
        
        esp_task_wdt_reset();
        yield();
    }
    
    authenticated = false;
    return true;
}

void FBase::loop() {
    if (!authenticated || !app.ready()) return;

    // WiFi caiu — cancela pendentes e marca para reconexão
    if (WiFi.status() != WL_CONNECTED) {
        _pendingFloat  = false;
        _pendingString = false;
        _pendingBool   = false;
        _lastBoolSet   = false;
        authenticated = false;
        return;
    }

    // app.loop() roda na firebaseTask (core 0) que tem watchdog próprio de 60s.
    // Se bloquear por erro SSL (-76/-80), o watchdog da task dispara — sem crash
    // no loopTask. O authenticated será marcado false pelo isHealthy() na task.
    app.loop();
    Database.loop();
}

bool FBase::isHealthy() 
{
    // Não verifica ssl_client.connected() — o socket de dados só abre
    // na primeira operação após o init(), causaria falso negativo imediato.
    return authenticated && app.ready() && WiFi.status() == WL_CONNECTED;
}

bool FBase::isReady() {
    return authenticated && app.ready();
}

bool FBase::isBusy()
{
    return _pendingFloat || _pendingString || _pendingBool;
}

bool FBase::hasCredentialError()
{
    return _credentialError;
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

// ─── Ponteiro estático para callbacks sem lambda ───────────────────────────────
FBase* FBase::_instance = NULL;

void FBase::_cbString(AsyncResult &result)
{
    if (!_instance) return;
    _instance->_pendingString = false;
    if (result.isError()) {
        Serial.printf("[Firebase] Erro string: %s\n", result.error().message().c_str());
        _instance->_lastStringPath[0]  = '\0';
        _instance->_lastStringValue[0] = '\0';
    }
}

void FBase::_cbFloat(AsyncResult &result)
{
    if (!_instance) return;
    _instance->_pendingFloat = false;
    if (result.isError()) {
        Serial.printf("[Firebase] Erro float: %s\n", result.error().message().c_str());
        _instance->_lastFloatPath[0] = '\0';
        _instance->_lastFloatValue   = -9999.0f;
    }
}

void FBase::_cbBool(AsyncResult &result)
{
    if (!_instance) return;
    _instance->_pendingBool = false;
    if (result.isError()) {
        Serial.printf("[Firebase] Erro bool: %s\n", result.error().message().c_str());
        _instance->_lastBoolSet = false;
    }
}

// ─── String ───────────────────────────────────────────────────────────────────

void FBase::aSyncSetString(String path, String value)
{
    if (!isReady() || _pendingString) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (strcmp(path.c_str(), _lastStringPath) == 0 &&
        strcmp(value.c_str(), _lastStringValue) == 0) return;

    _instance = this;
    _pendingString = true;
    strncpy(_lastStringPath,  path.c_str(),  sizeof(_lastStringPath)  - 1);
    strncpy(_lastStringValue, value.c_str(), sizeof(_lastStringValue) - 1);

    Database.set(aClient, path, value.c_str(), _cbString);

    unsigned long timeout   = millis();
    unsigned long lastYield = millis();

    // Não chama loop() aqui — evita reentrada quando executado pela firebaseTask.
    // O app.loop() já é chamado pelo ciclo principal da firebaseTask.
    while (_pendingString && millis() - timeout < 3000) {
        if (WiFi.status() != WL_CONNECTED || !authenticated) {
            _pendingString      = false;
            _lastStringPath[0]  = '\0';
            _lastStringValue[0] = '\0';
            return;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (_pendingString) {
        Serial.println("[Firebase] timeout string");
        _pendingString      = false;
        _lastStringPath[0]  = '\0';
        _lastStringValue[0] = '\0';
    }
}

// ─── Float ────────────────────────────────────────────────────────────────────

void FBase::aSyncSetFloat(String path, float value)
{
    if (!isReady() || _pendingFloat) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (strcmp(path.c_str(), _lastFloatPath) == 0 &&
        value == _lastFloatValue) return;

    _instance = this;
    _pendingFloat = true;
    strncpy(_lastFloatPath, path.c_str(), sizeof(_lastFloatPath) - 1);
    _lastFloatValue = value;

    Database.set(aClient, path, value, _cbFloat);

    unsigned long timeout   = millis();
    unsigned long lastYield = millis();

    while (_pendingFloat && millis() - timeout < 3000) {
        if (WiFi.status() != WL_CONNECTED || !authenticated) {
            _pendingFloat     = false;
            _lastFloatPath[0] = '\0';
            _lastFloatValue   = -9999.0f;
            return;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (_pendingFloat) {
        Serial.println("[Firebase] timeout float");
        _pendingFloat     = false;
        _lastFloatPath[0] = '\0';
        _lastFloatValue   = -9999.0f;
    }
}

// ─── Bool ─────────────────────────────────────────────────────────────────────

void FBase::aSyncSetBool(String path, bool value)
{
    if (!isReady() || _pendingBool) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (_lastBoolSet &&
        strcmp(path.c_str(), _lastBoolPath) == 0 &&
        value == _lastBoolValue) return;

    _instance = this;
    _pendingBool = true;
    strncpy(_lastBoolPath, path.c_str(), sizeof(_lastBoolPath) - 1);
    _lastBoolValue = value;
    _lastBoolSet   = true;

    Database.set(aClient, path, value, _cbBool);

    unsigned long timeout   = millis();
    unsigned long lastYield = millis();

    while (_pendingBool && millis() - timeout < 3000) {
        if (WiFi.status() != WL_CONNECTED || !authenticated) {
            _pendingBool = false;
            _lastBoolSet = false;
            return;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (_pendingBool) {
        Serial.println("[Firebase] timeout bool");
        _pendingBool = false;
        _lastBoolSet = false;
    }
}

void FBase::awaitGet(String& path, String *result)
{
    if (!isReady()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    const unsigned long GET_TIMEOUT = 8000;
    int retries = 2;

    // Detecta se a firebaseTask já está rodando.
    // Se não estiver (ex: durante o setup), chama app.loop() manualmente
    // para processar a resposta do Database.get — caso contrário bloqueia
    // indefinidamente pois ninguém drena o stack SSL.
    TaskHandle_t fbTaskHandle = xTaskGetHandle("firebaseTask");
    bool taskRunning = (fbTaskHandle != NULL);

    while (retries > 0) {
        if (WiFi.status() != WL_CONNECTED || !authenticated) return;

        esp_task_wdt_reset();
        unsigned long startTime = millis();

        if (!taskRunning) {
            // Setup: precisa drenar manualmente enquanto aguarda resposta
            // Inicia a operação assíncrona e processa até receber ou timeout
            *result = Database.get<String>(aClient, path.c_str());
        } else {
            // Runtime: firebaseTask já chama app.loop() — operação síncrona normal
            *result = Database.get<String>(aClient, path.c_str());
        }
        esp_task_wdt_reset();

        unsigned long elapsed = millis() - startTime;
        if (elapsed > GET_TIMEOUT) {
            Serial.printf("[Firebase] awaitGet timeout (%lums). Reconectando.\n", elapsed);
            authenticated = false;
            return;
        }

        if (result->length() > 0 || aClient.lastError().code() == 0) {
            break;
        }

        retries--;
        unsigned long delayStart = millis();
        while (millis() - delayStart < 300) {
            esp_task_wdt_reset();
            yield();
        }
    }
}

void FBase::awaitSet(String &path, String value, int type)
{
    if (!isReady()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    const unsigned long SET_TIMEOUT = 8000;
    int retries = 2;
    bool success = false;

    while (retries > 0 && !success) {
        if (WiFi.status() != WL_CONNECTED || !authenticated) return;

        esp_task_wdt_reset();
        unsigned long startTime = millis();

        if (type == INT)
            success = Database.set<int>(aClient, path.c_str(), value.toInt());
        else if (type == FLOAT)
            success = Database.set<float>(aClient, path.c_str(), value.toFloat());
        else if (type == STRING)
            success = Database.set<String>(aClient, path.c_str(), value.c_str());
        else if (type == BOOL)
            success = Database.set<bool>(aClient, path.c_str(), value.toInt());

        esp_task_wdt_reset();

        unsigned long elapsed = millis() - startTime;
        if (elapsed > SET_TIMEOUT) {
            Serial.printf("[Firebase] awaitSet timeout (%lums). Reconectando.\n", elapsed);
            authenticated = false;
            return;
        }

        if (!success) {
            retries--;
            unsigned long delayStart = millis();
            while (millis() - delayStart < 300) {
                esp_task_wdt_reset();
                yield();
            }
        }
    }
}

// ── Sobrecargas const char* — convertem e delegam para as versões String ──────

void FBase::awaitGet(const char* path, char* result, unsigned int resultSize)
{
    if (!path || !result || resultSize == 0) return;
    String pathStr(path);
    String resultStr;
    awaitGet(pathStr, &resultStr);
    strncpy(result, resultStr.c_str(), resultSize - 1);
    result[resultSize - 1] = '\0';
}

void FBase::aSyncSetString(const char* path, const char* value)
{
    if (!path || !value) return;
    aSyncSetString(String(path), String(value));
}

void FBase::aSyncSetFloat(const char* path, float value)
{
    if (!path) return;
    aSyncSetFloat(String(path), value);
}

void FBase::aSyncSetBool(const char* path, bool value)
{
    if (!path) return;
    aSyncSetBool(String(path), value);
}