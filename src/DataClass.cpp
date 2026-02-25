#include "DataClass.hpp"

DataClass::DataClass(Light *light, Preferences *preferences) : prefs(preferences)
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
    timer_or_sensor = SENSOR;
    soil_low_raw = NOT_DEFINED;
    soil_upper_raw = NOT_DEFINED;
    soil_calibrated_value = NOT_DEFINED;

    pumpFlow = 0;
       
    dayTime[0] = 0;
    dayTime[1] = 0;
    nightTime[0] = 0;
    nightTime[1] = 0;
    timer_or_watch = WATCH;

    WaterReserv = 0;
    water_raw_reading = NOT_DEFINED;
    waterCapacity= NOT_DEFINED;
    water_calibrated_value = 0;
    ReservWarning = 0;
    

    lightStatus = false;
    pumpStatus = false;
    coolerStatus = false;
    heaterStatus = false;
    humidStatus = false;
    dehumidStatus = false;

    isRunning = false;

    this->light = light;

    hasChange = "false";

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

float DataClass::getPumpFlow()
{
    return pumpFlow;
}

int DataClass::getSoilBehavior()
{
    return timer_or_sensor;
}


void DataClass::setSoilLow(int soil_low)
{
    soil_low_raw = soil_low;
}

void DataClass::setSoilUpper(int soil_upper)
{
    soil_upper_raw = soil_upper;
}

void DataClass::setSoilBehavior(int type)
{
    timer_or_sensor = type;
}

void DataClass::setPumpFlow(float new_flow)
{
    pumpFlow = new_flow;
}
void DataClass::setDayTime(int h, int m)
{
   
        dayTime[0] = h;
        dayTime[1] = m;
        if (light != nullptr) {
            light->setDay(h, m);
        }
        
}

void DataClass::setNightTime(int h, int m)
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

void DataClass::setWaterCapacity(float new_capacity)
{
    waterCapacity = new_capacity;
}


void DataClass::setWaterRawReading(float raw_value)
{
    water_raw_reading = raw_value;
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

int DataClass::getSoilLow()
{
    return soil_low_raw;
}
int DataClass::getSoilUpper()
{
    return soil_upper_raw;
}

float DataClass::getCalibratedSoil()
{
    if (soil_low_raw == NOT_DEFINED || soil_upper_raw == NOT_DEFINED || soil_low_raw == soil_upper_raw)
        return 0.0f;

    float mapped_soil = map(getSoil(), getSoilLow(), getSoilUpper(), 0, 100);
    
    if(mapped_soil < 0)
        mapped_soil = 0;
    if(mapped_soil > 100)
        mapped_soil = 100;
        
    return mapped_soil;
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

void DataClass::getDayTime(int *out)
{
    out[0] = dayTime[0];
    out[1] = dayTime[1];
}

void DataClass::getNightTime(int *out)
{
    out[0] = nightTime[0];
    out[1] = nightTime[1];
}

float DataClass::getWaterRes()
{
    return WaterReserv;
}

float DataClass::getWaterCapacity()
{
    return waterCapacity;
}

float DataClass::getWaterRawReading()
{
    return water_raw_reading;
}

float DataClass::getWaterCalibrated()
{
    float raw = getWaterRes();
    float min = 60.0f;
    float max = getWaterRawReading();   // idealmente isso deveria ser fixo/calibrado

    if (max <= min)
        return 0.0f; // proteção contra divisão por zero

    float percent = (raw - min) * 100.0f / (max - min);

    // inverter escala (100 → 0)
    percent = 100.0f - percent;

    // limitar
    if (percent < 0.0f)
        percent = 0.0f;
    if (percent > 100.0f)
        percent = 100.0f;

    return percent;
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

bool DataClass::isSoilCalibrated()
{
    return soil_low_raw != -1 && soil_upper_raw != -1;
}

void DataClass::setIsRunning(bool stats)
{
   
        isRunning = stats;
        
}

bool DataClass::getIsRunning()
{
    return isRunning;
}

void DataClass::setHasChange(String new_status)
{
    hasChange = new_status;
}
String DataClass::getHasChange()
{
    return hasChange;
}
