#include "FBase.hpp"

FBase::FBase(FirebaseClient *firebase_client, const String& api, const String& db_url, const String& user_email, const String& user_password, 
             WiFiClientSecure *client_secure) : 
                firebaseClient(firebase_client), apiKey(api), dbUrl(db_url), email(user_email), password(user_password), clientSecure(client_secure),
                user_auth(api.c_str(), user_email.c_str(), user_password.c_str()),
                aClient(*client_secure), authenticated(false){}

bool FBase::init()
{
    clientSecure->setInsecure();

    initializeApp(aClient, app, getAuth(user_auth), nullptr, "authTask");
    
    if(app.ready())
    {
        authenticated = true;

        app.getApp<RealtimeDatabase>(Database);
        Database.url(dbUrl.c_str());

        return true;
    }
    else
    {
        authenticated = false;
        return false;
    }

}

bool FBase::isReady()
{
    return app.ready() && authenticated;
}

void FBase::run()
{
    app.loop();
    Database.loop();
}

String* FBase::asyncTarget = nullptr;

void FBase::asyncCallback(AsyncResult &aResult)
{
    if (!asyncTarget) return;

    if (aResult.isResult() && aResult.available())
        *asyncTarget = aResult.c_str();
}

void FBase::aSyncGet(const String& path, String &result)
{
    if (!isReady()) return;
    

    // salva onde o callback deve escrever
    asyncTarget = &result;

    Database.get(aClient, path.c_str(), asyncCallback, false, "aSyncGetTask");
}


void FBase::aSyncSet(const String &path, String &value)
{
    if (!isReady()) return;

    Database.set(aClient, path, value.c_str());
}

String FBase::awaitGet(const String& path)
{
    String res = "";

    res = Database.get(aClient, path.c_str());
    
    return res;
}
void FBase::awaitSet(const String &path, String &value, int type)
{
    if(type == INT)
        Database.set<int>(aClient, path.c_str(), value.toInt());
    if(type == FLOAT)
        Database.set<float>(aClient, path.c_str(), value.toFloat());
    if(type == STRING)
        Database.set<String>(aClient, path.c_str(), value.c_str());
    if(type == BOOL)
        Database.set<bool>(aClient, path.c_str(), value.toInt());
}

