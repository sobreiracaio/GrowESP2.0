#pragma once

#include "Libraries.hpp"
#include "FBase.hpp"
#include "Display.hpp"

class FBase;
class Display;

extern bool isOTA;

class OTA {
    private:
        String binaryUrl;
        String token;

        volatile size_t totalSize;    // tamanho total do firmware
        volatile size_t currentSize;  // quanto já foi gravado
        
        int serialNum;
        int digit1;
        int digit2;
        int digit3;

        String version;


        Preferences *prefs;
        FBase *firebase;
        Display *display;
    

    public:
        OTA(Preferences *prefs, FBase *firebase, Display *display);

        void setSerialNum();
        int getSerialNum();
        void setDigit(int position, int value);
        int getDigit(int position);

        void setBinaryPath(String path);
        void setToken(String token);
        void setVersion(String new_version);

        String getVersion();

        void injectFbase(FBase *firebase);
        void injectDisplay(Display *display);
        int updateDevice();
};