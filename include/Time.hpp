#pragma once

#include "Libraries.hpp"
#include "WifiManager.hpp"

class WifiManager;

class Time {

    private:
        const char* ntpServer;
        const char* ntpServer1;
        long gmtOffsetSec;
        int daylightOffsetSec;
        struct tm timeinfo;
        WifiManager *_wifi;
       

        unsigned long lastSyncMillis = 0;     // último sync NTP
        unsigned long syncInterval = 3600000; // 1 hora padrão em ms

    public:
        Time(WifiManager *wifi, const char* server = "pool.ntp.org", const char* server1 = "pool.ntp.br", long gmtOffset = 0, int daylightOffset = 0);

        bool begin();                  // Inicializa NTP e RTC interno
        bool update();                 // Atualiza struct tm do RTC
        String getTimeString();        // Hora formatada HH:MM:SS
        struct tm getTimeStruct();     // Retorna struct tm completo

        int getHour();
        int getMinute();
        int getSecond();
        int getDay();
        int getMonth();
        int getYear();

        // ⚡ Sincroniza NTP (chame periodicamente, ex: 1 vez/hora)
        void syncNTP();
        void checkSync(unsigned long intervalMs = 3600000);

        void setgmtOffSet(int timezone);
        long getTimeZone();

        bool getStatus();
        //struct tm getNow();
};