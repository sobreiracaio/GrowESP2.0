#pragma once

#include "Libraries.hpp"

class Display;

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

        String receivedData;

        Display *tft;
        CFastLED *leds;

             

        static void firebaseCallback(AsyncResult &aResult);

    public:

        FBase(FirebaseClient *firebase_client, const String& api, const String& db_url, const String& user_email, const String& user_password, WiFiClientSecure *client_secure, Display *display, CFastLED *led);

};