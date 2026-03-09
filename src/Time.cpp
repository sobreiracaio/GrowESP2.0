

#include "Time.hpp"

Time::Time(WifiManager *wifi, const char* server, const char* server1, long gmtOffset, int daylightOffset)
    : _wifi(wifi), ntpServer(server), ntpServer1(server1), gmtOffsetSec(gmtOffset), daylightOffsetSec(daylightOffset)  
    {
        timeinfo = {0};
    }

bool Time::begin() {
    // Sincronização inicial via NTP
    if(_wifi->getStatus() == false)
        return false;

    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer, ntpServer1);
    //Serial.println("Aguardando hora NTP...");
    
    unsigned long startMillis = millis();
    const unsigned long timeout = 10000; // Timeout de 10 segundos
    
    while (!getLocalTime(&timeinfo)) {
        //Serial.print(".");
        
        if (millis() - startMillis >= timeout) {
            //Serial.println("\nTimeout ao sincronizar hora NTP!");
            return false;
        }
    }
    
    //Serial.println("\nHora sincronizada!");
    return true;
}

bool Time::update() {
    if(_wifi->getStatus())
        return getLocalTime(&timeinfo);
    return false;
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

void Time::syncNTP() 
{
    // Não chama configTime() de novo — só atualiza timeinfo
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
     // Já sincronizado — não chama begin() de novo
    if (timeinfo.tm_year > 0)
        return true;

    // Só tenta sincronizar se ainda não tem hora
    return begin();
}

void Time::setgmtOffSet(int timezone)
{
    gmtOffsetSec = timezone * 3600;
}

long Time::getTimeZone()
{
    return gmtOffsetSec / 3600;
}

// struct tm Time::getNow()
// {
//     struct tm now = {0};
//     now.tm_hour = getHour();
//     now.tm_min = getMinute();
//     now.tm_sec = getSecond();
//     now.tm_mday = getDay();
//     now.tm_mon = getMonth();
//     now.tm_year = getYear();
//     checkSync();

//     return now;
// }