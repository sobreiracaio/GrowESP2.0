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
    waterCapacity = NOT_DEFINED;
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

    vpd = 0;
}

void DataClass::setTemp(float temp)          { temperature = temp; }
void DataClass::setTargetTemp(float v)       { targetTemp = v; }
void DataClass::setTempTolerance(float v)    { tempTolerance = v; }
void DataClass::setHumid(float v)            { hum = v; }
void DataClass::setTargetHumid(float v)      { targetHumid = v; }
void DataClass::setHumidTolerance(float v)   { humidTolerance = v; }
void DataClass::setSoil(float v)             { soilHum = v; }
void DataClass::setTargetSoil(float v)       { targetSoil = v; }
void DataClass::setSoilTolerance(float v)    { soilTolerance = v; }
void DataClass::setPumpDuration(float v)     { pumpDuration = v; }
void DataClass::setAbsorptionDelay(float v)  { absorptionDelay = v; }
void DataClass::setSoilLow(int v)            { soil_low_raw = v; }
void DataClass::setSoilUpper(int v)          { soil_upper_raw = v; }
void DataClass::setSoilBehavior(int v)       { timer_or_sensor = v; }
void DataClass::setPumpFlow(float v)         { pumpFlow = v; }

void DataClass::setDayTime(int h, int m)
{
    dayTime[0] = h;
    dayTime[1] = m;
    if (light != nullptr) light->setDay(h, m);
}

void DataClass::setNightTime(int h, int m)
{
    nightTime[0] = h;
    nightTime[1] = m;
    if (light != nullptr) light->setNight(h, m);
}

void DataClass::setWaterRes(float v)         { WaterReserv = v; }
void DataClass::setWaterCapacity(float v)    { waterCapacity = v; }
void DataClass::setWaterRawReading(float v)  { water_raw_reading = v; }
void DataClass::setReservWarning(float v)    { ReservWarning = v; }
void DataClass::setLightStatus(bool v)       { lightStatus = v; }
void DataClass::setPumpStatus(bool v)        { pumpStatus = v; }
void DataClass::setCoolerStatus(bool v)      { coolerStatus = v; }
void DataClass::setHeaterStatus(bool v)      { heaterStatus = v; }
void DataClass::setHumidStatus(bool v)       { humidStatus = v; }
void DataClass::setDehumidStatus(bool v)     { dehumidStatus = v; }
void DataClass::setIsRunning(bool v)         { isRunning = v; }
void DataClass::setHasChange(String v)       { hasChange = v; }

float DataClass::getTemp()           { return roundf(temperature * 4.0f) / 4.0f; }
float DataClass::getTargetTemp()     { return targetTemp; }
float DataClass::getTempTolerance()  { return tempTolerance; }
float DataClass::getHumid()          { return roundf(hum * 4.0f) / 4.0f; }
float DataClass::getTargetHumid()    { return targetHumid; }
float DataClass::getHumidTolerance() { return humidTolerance; }
float DataClass::getSoil()           { return soilHum; }
int   DataClass::getSoilLow()        { return soil_low_raw; }
int   DataClass::getSoilUpper()      { return soil_upper_raw; }
float DataClass::getTargetSoil()     { return targetSoil; }
float DataClass::getSoilTolerance()  { return soilTolerance; }
float DataClass::getPumpDuration()   { return pumpDuration; }
float DataClass::getAbsorptionDelay(){ return absorptionDelay; }
float DataClass::getPumpFlow()       { return pumpFlow; }
int   DataClass::getSoilBehavior()   { return timer_or_sensor; }

float DataClass::getCalibratedSoil()
{
    if (soil_low_raw == NOT_DEFINED || soil_upper_raw == NOT_DEFINED || soil_low_raw == soil_upper_raw)
        return 0.0f;
    float mapped = map(getSoil(), getSoilLow(), getSoilUpper(), 0, 100);
    if (mapped < 0)   mapped = 0;
    if (mapped > 100) mapped = 100;
    return mapped;
}

void DataClass::getDayTime(int *out)   { out[0] = dayTime[0];   out[1] = dayTime[1]; }
void DataClass::getNightTime(int *out) { out[0] = nightTime[0]; out[1] = nightTime[1]; }

float DataClass::getWaterRes()         { return WaterReserv; }
float DataClass::getWaterCapacity()    { return waterCapacity; }
float DataClass::getWaterRawReading()  { return water_raw_reading; }
float DataClass::getReservWarning()    { return ReservWarning; }

float DataClass::getWaterCalibrated()
{
    float raw = getWaterRes();
    float mn  = 60.0f;
    float mx  = getWaterRawReading();
    if (mx <= mn) return 0.0f;
    float pct = 100.0f - (raw - mn) * 100.0f / (mx - mn);
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return roundf(pct * 4.0f) / 4.0f;
}

bool DataClass::getLightStatus()    { return lightStatus; }
bool DataClass::getPumpStatus()     { return pumpStatus; }
bool DataClass::getCoolerStatus()   { return coolerStatus; }
bool DataClass::getHeaterStatus()   { return heaterStatus; }
bool DataClass::getHumidStatus()    { return humidStatus; }
bool DataClass::getDehumidStatus()  { return dehumidStatus; }
bool DataClass::isSoilCalibrated()  { return soil_low_raw != -1 && soil_upper_raw != -1; }
bool DataClass::getIsRunning()      { return isRunning; }
String DataClass::getHasChange()    { return hasChange; }

float DataClass::getVPD()
{
    float svp = 0.6108f * exp((17.27f * temperature) / (temperature + 237.3f));
    return svp * (1.0f - (hum / 100.0f));
}

// ── saveToPrefs ───────────────────────────────────────────────────────────────
void DataClass::saveToPrefs()
{
    prefs->begin("idata", false);
    prefs->putFloat("targetTemp",   targetTemp);
    prefs->putFloat("tempTol",      tempTolerance);
    prefs->putFloat("targetHumid",  targetHumid);
    prefs->putFloat("humidTol",     humidTolerance);
    prefs->putFloat("targetSoil",   targetSoil);
    prefs->putFloat("soilTol",      soilTolerance);
    prefs->putFloat("pumpDuration", pumpDuration);
    prefs->putFloat("absDelay",     absorptionDelay);
    prefs->putFloat("pumpFlow",     pumpFlow);
    prefs->putInt  ("soilBehavior", timer_or_sensor);
    prefs->putInt  ("soilLow",      soil_low_raw);
    prefs->putInt  ("soilUpper",    soil_upper_raw);
    prefs->putFloat("waterRaw",     water_raw_reading);
    prefs->putFloat("waterCap",     waterCapacity);
    prefs->putInt  ("dayH",         dayTime[0]);
    prefs->putInt  ("dayM",         dayTime[1]);
    prefs->putInt  ("nightH",       nightTime[0]);
    prefs->putInt  ("nightM",       nightTime[1]);
    prefs->putBool ("isRunning",    isRunning);
    prefs->end();
}

// ── loadFromPrefs ─────────────────────────────────────────────────────────────
void DataClass::loadFromPrefs()
{
    prefs->begin("idata", true);

    if (!prefs->isKey("targetTemp")) {
        prefs->end();
        Serial.println("[DataClass] Nenhum dado salvo, usando defaults.");
        return;
    }

    targetTemp      = prefs->getFloat("targetTemp",   targetTemp);
    tempTolerance   = prefs->getFloat("tempTol",      tempTolerance);
    targetHumid     = prefs->getFloat("targetHumid",  targetHumid);
    humidTolerance  = prefs->getFloat("humidTol",     humidTolerance);
    targetSoil      = prefs->getFloat("targetSoil",   targetSoil);
    soilTolerance   = prefs->getFloat("soilTol",      soilTolerance);
    pumpDuration    = prefs->getFloat("pumpDuration", pumpDuration);
    absorptionDelay = prefs->getFloat("absDelay",     absorptionDelay);
    pumpFlow        = prefs->getFloat("pumpFlow",     pumpFlow);
    timer_or_sensor = prefs->getInt  ("soilBehavior", timer_or_sensor);
    soil_low_raw    = prefs->getInt  ("soilLow",      soil_low_raw);
    soil_upper_raw  = prefs->getInt  ("soilUpper",    soil_upper_raw);
    water_raw_reading = prefs->getFloat("waterRaw",   water_raw_reading);
    waterCapacity   = prefs->getFloat("waterCap",     waterCapacity);
    dayTime[0]      = prefs->getInt  ("dayH",         dayTime[0]);
    dayTime[1]      = prefs->getInt  ("dayM",         dayTime[1]);
    nightTime[0]    = prefs->getInt  ("nightH",       nightTime[0]);
    nightTime[1]    = prefs->getInt  ("nightM",       nightTime[1]);
    isRunning       = prefs->getBool ("isRunning",    isRunning);
    prefs->end();

    if (light != nullptr) {
        light->setDay(dayTime[0], dayTime[1]);
        light->setNight(nightTime[0], nightTime[1]);
    }

    Serial.println("[DataClass] Dados carregados das Preferences.");
}