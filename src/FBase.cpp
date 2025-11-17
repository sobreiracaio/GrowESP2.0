#include "FBase.hpp"

FBase::FBase(FirebaseClient *firebase_client, const String& api, const String& db_url, const String& user_email, const String& user_password, 
             WiFiClientSecure *client_secure, Display *display) : 
                firebaseClient(firebase_client), apiKey(api), dbUrl(dbUrl), email(user_email), password(user_password), clientSecure(client_secure),
                tft(display), user_auth(api.c_str(), user_email.c_str(), user_password.c_str()),
                aClient(*client_secure), authenticated(false){}