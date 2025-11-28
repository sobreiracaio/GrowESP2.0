#include "FBase.hpp"

FBase::FBase(const String& api, const String& db_url, const String& user_email, const String& user_password)
    : apiKey(api), 
      dbUrl(db_url), 
      email(user_email), 
      password(user_password),
      user_auth(api.c_str(), user_email.c_str(), user_password.c_str()),
      aClient(ssl_client),
      authenticated(false) 
      {}

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

bool FBase::init() {
    Serial.println("\n=== 🔥 Inicializando Firebase ===");
    Serial.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    


    // Configurar SSL
    ssl_client.setInsecure();
    
    // Inicializar o app com autenticação
    Serial.println("🔐 Autenticando...");
    initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
    
    // Aguardar autenticação (máximo 30 segundos)
    unsigned long timeout = millis();
    while (app.isInitialized() && !app.ready() && millis() - timeout < 30000) {
        app.loop();
        
    }
    
    if (app.ready()) {
        Serial.println("✅ Firebase autenticado com sucesso!");
        authenticated = true;
        
        // Inicializar Realtime Database
        app.getApp<RealtimeDatabase>(Database);
        Database.url(dbUrl.c_str());
        
        Serial.println("✅ Realtime Database configurado!");
        Serial.println("=================================\n");
        return true;
    } else {
        Serial.println("❌ Erro na autenticação do Firebase!");
        Serial.println("=================================\n");
        authenticated = false;
        return false;
    }
}

void FBase::loop() {
    app.loop();
    Database.loop();
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


void FBase::aSyncSet(String &path, String &value)
{
    if (!isReady()) return;

    Database.set(aClient, path, value.c_str());
}

String FBase::awaitGet(String& path)
{
    if (!isReady()) return "";
    String res = "";

    res = Database.get(aClient, path.c_str());

    return res;
}
void FBase::awaitSet(String &path, String value, int type)
{
    if (!isReady()) return;
    if(type == INT)
        Database.set<int>(aClient, path.c_str(), value.toInt());
    if(type == FLOAT)
        Database.set<float>(aClient, path.c_str(), value.toFloat());
    if(type == STRING)
        Database.set<String>(aClient, path.c_str(), value.c_str());
    if(type == BOOL)
        Database.set<bool>(aClient, path.c_str(), value.toInt());
}

