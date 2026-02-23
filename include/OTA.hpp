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
        
        String version;
        bool hasUpdate;
        bool isUpdated;

        Preferences *prefs;
        FBase *firebase;
        Display *display;

        String releaseTag = "";
        String releaseName = "";
        String releaseBody = "";
        String releaseDate = "";
    

    public:
        OTA(Preferences *prefs, FBase *firebase, Display *display);

        bool checkForUpdates();

        void setBinaryPath(String path);
        void setToken(String token);
        void setVersion(String new_version);
        void setHasUpdate(bool status);

        bool getHasUpdate();
        String getVersion();

        void injectFbase(FBase *firebase);
        void injectDisplay(Display *display);
        int updateDevice();

        int fetchReleaseInfo();
        String getReleaseTag();  
        String getReleaseName();
        String getReleaseBody();
        String getReleaseDate();
};