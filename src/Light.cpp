#include "Light.hpp"

Light::Light() : dayTime({0}), nightTime({0}), getNowFunc(nullptr)
{
    dayTime.tm_isdst = -1;
    nightTime.tm_isdst = -1;
}

void Light::run(bool isRunning)
{
    if(getNowFunc == nullptr) {
        status = OFF;
        return;
    }

    if(isRunning == false)
    {
        status = OFF;
        return;
    }
    
    // Atualiza o tempo atual através da função registrada
    getNowFunc();

    // Atualiza dayTime e nightTime com a data atual
    dayTime.tm_year = now.tm_year;
    dayTime.tm_mon = now.tm_mon;
    dayTime.tm_mday = now.tm_mday;
    
    nightTime.tm_year = now.tm_year;
    nightTime.tm_mon = now.tm_mon;
    nightTime.tm_mday = now.tm_mday;
    
    time_t day = mktime(&dayTime);
    time_t night = mktime(&nightTime);
    
    // Se night <= day, significa que o período atravessa meia-noite
    // Então nightTime deve ser no dia seguinte
    if(night <= day) {
        nightTime.tm_mday++;
        night = mktime(&nightTime);
    }

    time_t realTime = mktime(&now);

    // Verifica se está no período de luz
    status = (realTime >= day && realTime < night) ? ON : OFF;
}

void Light::setTimeFunction(void (*func)())
{
    getNowFunc = func;
}

void Light::setDay(int h, int m)
{
    dayTime.tm_hour = h;
    dayTime.tm_min = m;
    dayTime.tm_sec = 0;
    dayTime.tm_isdst = -1;
    
    // Usa a data atual de 'now' se disponível
    dayTime.tm_year = now.tm_year;
    dayTime.tm_mon = now.tm_mon;
    dayTime.tm_mday = now.tm_mday;
}

void Light::setNight(int h, int m)
{
    nightTime.tm_hour = h;
    nightTime.tm_min = m;
    nightTime.tm_sec = 0;
    nightTime.tm_isdst = -1;
    
    // Usa a data atual de 'now' se disponível
    nightTime.tm_year = now.tm_year;
    nightTime.tm_mon = now.tm_mon;
    nightTime.tm_mday = now.tm_mday;
}

struct tm Light::getDay()
{
    return dayTime;
}

struct tm Light::getNight()
{
    return nightTime;
}