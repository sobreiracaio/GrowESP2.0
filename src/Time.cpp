

#include "Time.hpp"

Time::Time(const char* server, const char* server1, long gmtOffset, int daylightOffset)
    : ntpServer(server), ntpServer1(server1), gmtOffsetSec(gmtOffset), daylightOffsetSec(daylightOffset) {}

bool Time::begin() {
    // Sincronização inicial via NTP
    
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer, ntpServer1);
    //Serial.println("Aguardando hora NTP...");
    while (!getLocalTime(&timeinfo)) {
        //Serial.print(".");
    }
    //Serial.println("\nHora sincronizada!");
    return true;
}

bool Time::update() {
    // Atualiza struct tm usando RTC interno
    return getLocalTime(&timeinfo);
}

String Time::getTimeString() {
    if (!update()) return "Hora indisponível";
    char buffer[9];
    sprintf(buffer, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buffer);
}

struct tm Time::getTimeStruct() {
    update();
    return timeinfo;
}

// =========================
// Métodos para hora/minuto/segundo separados
// =========================

int Time::getHour() {
    if (update()) return timeinfo.tm_hour;
    return -1;
}

int Time::getMinute() {
    if (update()) return timeinfo.tm_min;
    return -1;
}

int Time::getSecond() {
    if (update()) return timeinfo.tm_sec;
    return -1;
}

int Time::getDay()
{
    if (update()) return timeinfo.tm_mday;
    return -1;
}

int Time::getMonth()
{
    if (update()) return timeinfo.tm_mon;
    return -1;
}

int Time::getYear()
{
    if (update()) return timeinfo.tm_year;
    return -1;
}

// =========================
// Método para sincronização NTP periódica
// =========================

void Time::syncNTP() {
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);
    getLocalTime(&timeinfo);
    lastSyncMillis = millis();
}

void Time::checkSync(unsigned long intervalMs) {
    if (millis() - lastSyncMillis >= intervalMs) {
        syncNTP();
    }
}


bool Time::getStatus()
{
    if(begin())
        return true;
    return false;
}