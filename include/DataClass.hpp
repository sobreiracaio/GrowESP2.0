#pragma once

#include "Predefinitions.hpp"
#include "Libraries.hpp"
#include "Light.hpp"


#define WATCH 0
#define TIMER 1
#define SENSOR 2

#define NOT_DEFINED -1

class Light;


class DataClass {
    
    private:

        Preferences *prefs;

        bool isRunning;
        
        //Temperatura

        float temperature;
        float targetTemp;
        float tempTolerance;

        //Umidade

        float hum;
        float targetHumid;
        float humidTolerance;

        //Solo e rega

        float soilHum;
        float targetSoil;
        float soilTolerance;
        float pumpDuration;
        float absorptionDelay;

        int  timer_or_sensor;
        int  soil_low_raw;
        int  soil_upper_raw;
        float pumpFlow;
        int soil_calibrated_value;

        //Fotoperiodo
        
        int timer_or_watch;
        int dayTime[2];
        int nightTime[2];
        
        //Reservatorios
        
        float WaterReserv;
        float water_raw_reading;
        float waterCapacity;
        float water_calibrated_value;
        float ReservWarning;
        
        //Atuadores
        
        bool lightStatus;
        bool pumpStatus;
        bool coolerStatus;
        bool heaterStatus;
        bool humidStatus;
        bool dehumidStatus;
        
        Light *light;
        
        public:
        
        DataClass(Light *light, Preferences *preferences);
        
        void setTemp(float temp);
        void setTargetTemp(float target_temp);
        void setTempTolerance(float temp_tol);
        
        void setHumid(float humid);
        void setTargetHumid(float target_hum);
        void setHumidTolerance(float hum_tol);
        
        void setSoil(float soil);
        void setSoilLow(int soil_low);
        void setSoilUpper(int soil_upper);
        void setTargetSoil(float target_soil);
        void setSoilTolerance(float soil_tol);
        void setPumpDuration(float pump_duration);
        void setAbsorptionDelay(float abs_delay);
        void setSoilBehavior(int type);
        void setPumpFlow(float new_flow);
        
        void setDayTime(int h, int m);
        void setNightTime(int h, int m);
        void setLightBehavior(int type);
        
        void setWaterRes(float value);
        
        void setReservWarning(float value);
        void setWaterCapacity(float new_capacity);
        void setWaterRawReading(float raw_value);
        
        void setLightStatus(bool status);
        void setPumpStatus(bool status);
        void setCoolerStatus(bool status);
        void setHeaterStatus(bool status);
        void setHumidStatus(bool status);
        void setDehumidStatus(bool status);
        
        
        float getTemp();
        float getTargetTemp();
        float getTempTolerance();
        float getHumid();
        float getTargetHumid();
        float getHumidTolerance();
        float getSoil();
        int getSoilLow();
        int getSoilUpper();
        float getCalibratedSoil();
        float getTargetSoil();
        float getSoilTolerance();
        float getPumpDuration();
        float getAbsorptionDelay();
        float getPumpFlow();
        int getSoilBehavior();
        
        void getDayTime(int *out);
        void getNightTime(int *out);
        
        float getWaterRes();
        float getWaterCapacity();
        float getWaterRawReading();
        float getWaterCalibrated();
        float getReservWarning();
        
        bool getLightStatus();
        bool getPumpStatus();
        bool getCoolerStatus();
        bool getHeaterStatus();
        bool getHumidStatus();
        bool getDehumidStatus();
        bool isSoilCalibrated();
        
        void setIsRunning(bool stats);
        bool getIsRunning();
        
            
        
        
        
    };