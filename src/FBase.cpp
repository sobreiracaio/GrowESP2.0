#include "FBase.hpp"

FBase::FBase(FirebaseClient *firebase_client, const String& api, const String& db_url, const String& user_email, const String& user_password, 
             WiFiClientSecure *client_secure) : 
                firebaseClient(firebase_client), apiKey(api), dbUrl(dbUrl), email(user_email), password(user_password), clientSecure(client_secure),
                user_auth(api.c_str(), user_email.c_str(), user_password.c_str()),
                aClient(*client_secure), authenticated(false){}