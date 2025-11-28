#pragma once

#define ENABLE_DATABASE
#define ENABLE_USER_AUTH

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

#define API_KEY "AIzaSyBZOxkbUT3b9COdhCqNul3kNy6HEuSU5S4"
#define DATABASE_URL "https://growstation-183df-default-rtdb.firebaseio.com"

#define INT 0
#define FLOAT 1
#define STRING 2
#define BOOL 3

class FBase {

    private:
        String apiKey;
        String dbUrl;
        String email;
        String password;
        
        UserAuth user_auth;
        FirebaseApp app;
        WiFiClientSecure ssl_client;
        AsyncClientClass aClient;
        RealtimeDatabase Database;

        bool authenticated;

        
        static void processData(AsyncResult &aResult);

        static String *asyncTarget;
     
    public:

        FBase(const String& api, const String& db_url, const String& user_email, const String& user_password);
        bool init();
        bool isReady();
        void loop();

        String awaitGet(String& path);
        void awaitSet(String &path, String value, int type);

        void aSyncSet(String &path, String &value);

        void aSyncGet(String& path, String &result);
        static void asyncCallback(AsyncResult &aResult);
};

