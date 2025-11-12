#pragma once

#include "Libraries.hpp"

class Time {

    private:
        const char* ntpServer;
        long gmtOffsetSec;
        int daylightOffsetSec;
        struct tm timeinfo;

        unsigned long lastSyncMillis = 0;     // último sync NTP
        unsigned long syncInterval = 3600000; // 1 hora padrão em ms

    public:
        Time(const char* server = "pool.ntp.org", long gmtOffset = 0, int daylightOffset = 0);

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

        bool getStatus();
};