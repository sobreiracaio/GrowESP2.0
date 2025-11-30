#include "DataClass.hpp"

DataClass::DataClass(Light *light)
{

    
    temperature = 0;
    targetTemp = 30;
    tempTolerance = 2;

    hum = 0;
    targetHumid = 25;
    humidTolerance = 2;

    soilHum = 0;
    targetSoil = 75;
    soilTolerance = 5;

        
    pumpDuration = 5;
    absorptionDelay = 30;
       
    dayTime[0] = 0;
    dayTime[1] = 0;
    nightTime[0] = 0;
    nightTime[1] = 0;

    WaterReserv = 0;
    HumidReserv = 0;
    ReservWarning = 20;

    lightStatus = false;
    pumpStatus = false;
    coolerStatus = false;
    heaterStatus = false;
    humidStatus = false;
    dehumidStatus = false;

    isRunning = false;

    this->light = light;

}





void DataClass::setTemp(float temp)
{
   
        temperature = temp;
   
}

void DataClass::setTargetTemp(float target_temp)
{
   
        targetTemp = target_temp;
  
}

void DataClass::setTempTolerance(float temp_tol)
{
   
        tempTolerance = temp_tol;
        
}

void DataClass::setHumid(float humidity)
{
   
        hum = humidity;
        
}

void DataClass::setTargetHumid(float target_hum)
{
   
        targetHumid = target_hum;
        
}

void DataClass::setHumidTolerance(float hum_tol)
{
   
        humidTolerance = hum_tol;
        
}

void DataClass::setSoil(float soil)
{
   
        soilHum = soil;
        
}

void DataClass::setTargetSoil(float target_soil)
{
   
        targetSoil = target_soil;
        
}

void DataClass::setSoilTolerance(float soil_tol)
{
   
        soilTolerance = soil_tol;
        
}

void DataClass::setPumpDuration(float pump_duration)
{
   
        pumpDuration = pump_duration;
        
}

void DataClass::setAbsorptionDelay(float abs_delay)
{
   
        absorptionDelay = abs_delay;
        
}

void DataClass::setDayTime(float h, float m)
{
   
        dayTime[0] = h;
        dayTime[1] = m;
        if (light != nullptr) {
            light->setDay(h, m);
        }
        
}

void DataClass::setNightTime(float h, float m)
{
   
        nightTime[0] = h;
        nightTime[1] = m;
        if (light != nullptr) {
            light->setNight(h, m);
        }
        
}

void DataClass::setWaterRes(float value)
{
   
        WaterReserv = value;
        
}

void DataClass::setHumidRes(float value)
{
   
        HumidReserv = value;
        
}

void DataClass::setReservWarning(float value)
{
   
        ReservWarning = value;
        
}

void DataClass::setLightStatus(bool status)
{
   
        lightStatus = status;
        
}

void DataClass::setPumpStatus(bool status)
{
   
        pumpStatus = status;
        
}

void DataClass::setCoolerStatus(bool status)
{
   
        coolerStatus = status;
        
}

void DataClass::setHeaterStatus(bool status)
{
   
        heaterStatus = status;
        
}

void DataClass::setHumidStatus(bool status)
{
   
        humidStatus = status;
        
}

void DataClass::setDehumidStatus(bool status)
{
   
        dehumidStatus = status;
        
}


//GETTERS

float DataClass::getTemp()
{
    return temperature;
}

float DataClass::getTargetTemp()
{
    return targetTemp;
}

float DataClass::getTempTolerance()
{
    return tempTolerance;
}

float DataClass::getHumid()
{
    return hum;
}

float DataClass::getTargetHumid()
{
    return targetHumid;
}

float DataClass::getHumidTolerance()
{
    return humidTolerance;
}

float DataClass::getSoil()
{
    return soilHum;
}

float DataClass::getTargetSoil()
{
    return targetSoil;
}

float DataClass::getSoilTolerance()
{
    return soilTolerance;
}

float DataClass::getPumpDuration()
{
    return pumpDuration;
}

float DataClass::getAbsorptionDelay()
{
    return absorptionDelay;
}

void DataClass::getDayTime(float *out)
{
    out[0] = dayTime[0];
    out[1] = dayTime[1];
}

void DataClass::getNightTime(float *out)
{
    out[0] = nightTime[0];
    out[1] = nightTime[1];
}

float DataClass::getWaterRes()
{
    return WaterReserv;
}

float DataClass::getHumidRes()
{
    return HumidReserv;
}

float DataClass::getReservWarning()
{
    return ReservWarning;
}

bool DataClass::getLightStatus()
{
    return lightStatus;
}

bool DataClass::getPumpStatus()
{
    return pumpStatus;
}

bool DataClass::getCoolerStatus()
{
    return coolerStatus;
}

bool DataClass::getHeaterStatus()
{
    return heaterStatus;
}

bool DataClass::getHumidStatus()
{
    return humidStatus;
}

bool DataClass::getDehumidStatus()
{
    return dehumidStatus;
}

void DataClass::setIsRunning(bool stats)
{
   
        isRunning = stats;
        
}

bool DataClass::getIsRunning()
{
    return isRunning;
}