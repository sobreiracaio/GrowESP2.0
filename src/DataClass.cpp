#include "DataClass.hpp"

DataClass::DataClass(Light *light)
{

    dataMutex = xSemaphoreCreateMutex();

    if(dataMutex == NULL)
        Serial.println("Erro ao criar mutex do DataBuild!");

    temperature = 0;
    targetTemp = 25;
    tempTolerance = 2;

    hum = 0;
    targetHumid = 50;
    humidTolerance = 2;

    soilHum = 0;
    targetSoil = 80;
    soilTolerance = 5;

        
    pumpDuration = 5;
    absorptionDelay = 30;
       
    dayTime[0] = 12;
    dayTime[1] = 0;
    nightTime[0] = 0;
    nightTime[1] = 0;

    WaterReserv = 0;
    HumidReserv = 0;

    lightStatus = false;
    pumpStatus = false;
    coolerStatus = false;
    heaterStatus = false;
    humidStatus = false;
    dehumidStatus = false;

    isRunning = false;

    this->light = light;

}

DataClass::~DataClass()
{
    if(dataMutex != NULL)
        vSemaphoreDelete(dataMutex);
}

bool DataClass::lock(TickType_t timeout)
{
    return xSemaphoreTake(dataMutex, timeout) == pdTRUE;
}

void DataClass::unlock()
{
    xSemaphoreGive(dataMutex);
}

void DataClass::setTemp(float temp)
{
    if (lock()) {
        temperature = temp;
        unlock();
    }
}

void DataClass::setTargetTemp(float target_temp)
{
    if (lock()) {
        targetTemp = target_temp;
        unlock();
    }
}

void DataClass::setTempTolerance(float temp_tol)
{
    if (lock()) {
        tempTolerance = temp_tol;
        unlock();
    }
}

void DataClass::setHumid(float humidity)
{
    if (lock()) {
        hum = humidity;
        unlock();
    }
}

void DataClass::setTargetHumid(float target_hum)
{
    if (lock()) {
        targetHumid = target_hum;
        unlock();
    }
}

void DataClass::setHumidTolerance(float hum_tol)
{
    if (lock()) {
        humidTolerance = hum_tol;
        unlock();
    }
}

void DataClass::setSoil(float soil)
{
    if (lock()) {
        soilHum = soil;
        unlock();
    }
}

void DataClass::setTargetSoil(float target_soil)
{
    if (lock()) {
        targetSoil = target_soil;
        unlock();
    }
}

void DataClass::setSoilTolerance(float soil_tol)
{
    if (lock()) {
        soilTolerance = soil_tol;
        unlock();
    }
}

void DataClass::setPumpDuration(float pump_duration)
{
    if (lock()) {
        pumpDuration = pump_duration;
        unlock();
    }
}

void DataClass::setAbsorptionDelay(float abs_delay)
{
    if (lock()) {
        absorptionDelay = abs_delay;
        unlock();
    }
}

void DataClass::setDayTime(float h, float m)
{
    if (lock()) {
        dayTime[0] = h;
        dayTime[1] = m;
        if (light != nullptr) {
            light->setDay(h, m);
        }
        unlock();
    }
}

void DataClass::setNightTime(float h, float m)
{
    if (lock()) {
        nightTime[0] = h;
        nightTime[1] = m;
        if (light != nullptr) {
            light->setNight(h, m);
        }
        unlock();
    }
}

void DataClass::setWaterRes(float value)
{
    if (lock()) {
        WaterReserv = value;
        unlock();
    }
}

void DataClass::setHumidRes(float value)
{
    if (lock()) {
        HumidReserv = value;
        unlock();
    }
}

void DataClass::setLightStatus(bool status)
{
    if (lock()) {
        lightStatus = status;
        unlock();
    }
}

void DataClass::setPumpStatus(bool status)
{
    if (lock()) {
        pumpStatus = status;
        unlock();
    }
}

void DataClass::setCoolerStatus(bool status)
{
    if (lock()) {
        coolerStatus = status;
        unlock();
    }
}

void DataClass::setHeaterStatus(bool status)
{
    if (lock()) {
        heaterStatus = status;
        unlock();
    }
}

void DataClass::setHumidStatus(bool status)
{
    if (lock()) {
        humidStatus = status;
        unlock();
    }
}

void DataClass::setDehumidStatus(bool status)
{
    if (lock()) {
        dehumidStatus = status;
        unlock();
    }
}


//GETTERS

float DataClass::getTemp()
{
    float val = 0;
    if (lock()) {
        val = temperature;
        unlock();
    }
    return val;
}

float DataClass::getTargetTemp()
{
    float val = 0;
    if (lock()) {
        val = targetTemp;
        unlock();
    }
    return val;
}

float DataClass::getTempTolerance()
{
    float val = 0;
    if (lock()) {
        val = tempTolerance;
        unlock();
    }
    return val;
}

float DataClass::getHumid()
{
    float val = 0;
    if (lock()) {
        val = hum;
        unlock();
    }
    return val;
}

float DataClass::getTargetHumid()
{
    float val = 0;
    if (lock()) {
        val = targetHumid;
        unlock();
    }
    return val;
}

float DataClass::getHumidTolerance()
{
    float val = 0;
    if (lock()) {
        val = humidTolerance;
        unlock();
    }
    return val;
}

float DataClass::getSoil()
{
    float val = 0;
    if (lock()) {
        val = soilHum;
        unlock();
    }
    return val;
}

float DataClass::getTargetSoil()
{
    float val = 0;
    if (lock()) {
        val = targetSoil;
        unlock();
    }
    return val;
}

float DataClass::getSoilTolerance()
{
    float val = 0;
    if (lock()) {
        val = soilTolerance;
        unlock();
    }
    return val;
}

float DataClass::getPumpDuration()
{
    float val = 0;
    if (lock()) {
        val = pumpDuration;
        unlock();
    }
    return val;
}

float DataClass::getAbsorptionDelay()
{
    float val = 0;
    if (lock()) {
        val = absorptionDelay;
        unlock();
    }
    return val;
}

void DataClass::getDayTime(float *out)
{
    if(lock())
    {
        out[0] = dayTime[0];
        out[1] = dayTime[1];
        unlock();
    }   
    
}

void DataClass::getNightTime(float *out)
{
    if(lock())
    {
        out[0] = nightTime[0];
        out[1] = nightTime[1];
        unlock();
    }   
}

float DataClass::getWaterRes()
{
    float val = 0;
    if (lock()) {
        val = WaterReserv;
        unlock();
    }
    return val;
}

float DataClass::getHumidRes()
{
    float val = 0;
    if (lock()) {
        val = HumidReserv;
        unlock();
    }
    return val;
}

float DataClass::getLightStatus()
{
    float val = 0;
    if (lock()) {
        val = lightStatus;
        unlock();
    }
    return val;
}

float DataClass::getPumpStatus()
{
    float val = false;
    if (lock()) {
        val = pumpStatus;
        unlock();
    }
    return val;
}

float DataClass::getCoolerStatus()
{
    float val = false;
    if (lock()) {
        val = coolerStatus;
        unlock();
    }
    return val;
}

float DataClass::getHeaterStatus()
{
    float val = false;
    if (lock()) {
        val = heaterStatus;
        unlock();
    }
    return val;
}

float DataClass::getHumidStatus()
{
    float val = false;
    if (lock()) {
        val = humidStatus;
        unlock();
    }
    return val;
}

float DataClass::getDehumidStatus()
{
    float val = false;
    if (lock()) {
        val = dehumidStatus;
        unlock();
    }
    return val;
}

void DataClass::setIsRunning(bool stats)
{
    if (lock()) {
        isRunning = stats;
        unlock();
    }
}

bool DataClass::getIsRunning()
{
    bool val = false;
    if (lock()) {
        val = isRunning;
        unlock();
    }
    return val;
}