#pragma once

#define ENABLE_DATABASE
#define ENABLE_USER_AUTH

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include "Libraries.hpp"


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
        bool _credentialError = false;  // true quando Firebase retorna erro de autenticação (email/senha inválidos)

        // flags e cache de operações pendentes
        bool  _pendingFloat  = false;
        bool  _pendingString = false;
        bool  _pendingBool   = false;
        bool  _lastBoolSet   = false;

        char  _lastStringPath[128]  = "";
        char  _lastStringValue[128] = "";
        char  _lastFloatPath[128]   = "";
        float _lastFloatValue       = -9999.0f;
        char  _lastBoolPath[128]    = "";
        bool  _lastBoolValue        = false;

        static void processData(AsyncResult &aResult);
        static String *asyncTarget;
        static FBase  *_instance;

        static void _cbString(AsyncResult &aResult);
        static void _cbFloat(AsyncResult &aResult);
        static void _cbBool(AsyncResult &aResult);
     
    public:

        FBase(const String& api, const String& db_url, const String& user_email, const String& user_password);
        bool init();
        bool stopApp();
        bool isReady();
        void loop();
        bool isHealthy();
        bool isBusy();
        bool hasCredentialError();

        void awaitGet(String& path, String *result);
        void awaitGet(const char* path, char* result, unsigned int resultSize);
        void awaitSet(String &path, String value, int type);

        void aSyncSetString(String path, String value);
        void aSyncSetString(const char* path, const char* value);
        void aSyncSetFloat(String path, float value);
        void aSyncSetFloat(const char* path, float value);
        void aSyncSetBool(String path, bool value);
        void aSyncSetBool(const char* path, bool value);

        void aSyncGet(String& path, String &result);
        static void asyncCallback(AsyncResult &aResult);
};