#pragma once

#include "Libraries.hpp"

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

        FirebaseClient *firebaseClient;
        UserAuth user_auth;
        FirebaseApp app;
        WiFiClientSecure *clientSecure;
        AsyncClientClass aClient;
        RealtimeDatabase Database;

        bool authenticated;

        static String *asyncTarget;
     
    public:

        FBase(FirebaseClient *firebase_client, const String& api, const String& db_url, const String& user_email, const String& user_password, WiFiClientSecure *client_secure);
        bool init();
        bool isReady();
        void run();

        String awaitGet(const String& path);
        void awaitSet(const String &path, String &value, int type);

        void aSyncSet(const String &path, String &value);

        void aSyncGet(const String& path, String &result);
        static void asyncCallback(AsyncResult &aResult);
};

