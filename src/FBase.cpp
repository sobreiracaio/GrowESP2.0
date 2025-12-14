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
    //Serial.println("\n=== 🔥 Inicializando Firebase ===");
    //Serial.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    ssl_client.setTimeout(30);

    if (ssl_client.connected()) 
    {
        ssl_client.stop();
        delay(100);
    }
    
    // Configurar SSL
    ssl_client.setInsecure();
    // Inicializar o app com autenticação
    //Serial.println("🔐 Autenticando...");
    initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
    
    // Aguardar autenticação (máximo 30 segundos)
    unsigned long timeout = millis();
    while (app.isInitialized() && !app.ready() && millis() - timeout < 60000) 
    {
        app.loop();
        delay(100);
    }
    
    if (app.ready()) {
        //Serial.println("✅ Firebase autenticado com sucesso!");
        authenticated = true;
        
        // Inicializar Realtime Database
        app.getApp<RealtimeDatabase>(Database);
        Database.url(dbUrl.c_str());
        aClient.setSyncReadTimeout(60);
        aClient.setSyncSendTimeout(60);
        
        return true;
    } else {
        if (ssl_client.connected()) 
            ssl_client.stop();
        
        authenticated = false;
        return false;
    }
}

bool FBase::stopApp()
{
    if (!authenticated) return true;
    
    aClient.stopAsync(true);
    
    // Garantir fechamento da conexão SSL
    if (ssl_client.connected()) {
        ssl_client.stop();

        // ===== intervalo não bloqueante (100ms) =====
        unsigned long start = millis();
        while (millis() - start < 100) {
            yield(); // mantém o WiFi/RTOS ativo
        }
        // ============================================
    }
    
    authenticated = false;
    return true;
}


void FBase::loop() {
    // ✅ Não faz nada se não autenticado
    if (!authenticated || !app.ready()) 
        return;

    // ✅ Verifica conexão SSL antes de operar
    if (ssl_client.connected()) {
        // ✅ Timeout razoável durante operações (5s)
        ssl_client.setTimeout(5000);
    } else {
        // ✅ Se desconectou, marca como não autenticado
        authenticated = false;
        return;
    }

    // ✅ Chama loops apenas se tudo OK
    app.loop();
    Database.loop();
}

// ✅ NOVA FUNÇÃO: Verifica saúde da conexão
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
    

    // salva onde o callback deve escrever
    asyncTarget = &result;
    Database.get(aClient, path.c_str(), asyncCallback, false, "aSyncGetTask");

}


void FBase::aSyncSetString(String &path, String &value)
{
    static String lastValue = "";
    if (!isReady() || lastValue == value) return;

    Database.set(aClient, path, value.c_str());
    lastValue = value;
}

void FBase::aSyncSetFloat(String &path, float &value)
{
    static float lastValue = 0;
    if (!isReady() || lastValue == value) return;

    Database.set(aClient, path, value);
    lastValue = value;
}

void FBase::aSyncSetBool(String &path, bool &value)
{
    
    if (!isReady()) return;

    Database.set(aClient, path, value);
    
}

void FBase::awaitGet(String& path, String *result)
{
    if (!isReady()) return;
    
    int retries = 3;
    unsigned long wait_start = 0;
    const unsigned long wait_time = 500; // 500ms

    while (retries > 0) {
        *result = Database.get<String>(aClient, path.c_str());

        if (result->length() > 0 || aClient.lastError().code() == 0) {
            break;
        }

        retries--;

        // === intervalo não bloqueante de 500 ms ===
        wait_start = millis();
        while (millis() - wait_start < wait_time) {
            // não bloqueia o programa — loop vazio
            // mas permite que outras coisas rodem no loop principal
            yield();
        }
        // =========================================
    }
}
void FBase::awaitSet(String &path, String value, int type)
{
    if (!isReady()) return;
    
    int retries = 3;
    bool success = false;
    
    while (retries > 0 && !success) {

        if(type == INT)
            success = Database.set<int>(aClient, path.c_str(), value.toInt());
        else if(type == FLOAT)
            success = Database.set<float>(aClient, path.c_str(), value.toFloat());
        else if(type == STRING)
            success = Database.set<String>(aClient, path.c_str(), value.c_str());
        else if(type == BOOL)
            success = Database.set<bool>(aClient, path.c_str(), value.toInt());
            
        if (!success) {
            retries--;

            // ===== intervalo não bloqueante (500ms) =====
            unsigned long start = millis();
            while (millis() - start < 500) {
                yield(); // permite WiFi/RTOS rodarem no ESP
            }
            // ============================================
        }
    }
}


