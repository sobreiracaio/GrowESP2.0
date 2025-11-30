#pragma once

#include "Predefinitions.hpp"
#include "Libraries.hpp"
#include "Light.hpp"

class Light;

class DataClass {
    
    private:

      

        bool isRunning;
        

        float temperature;
        float targetTemp;
        float tempTolerance;

        float hum;
        float targetHumid;
        float humidTolerance;

        float soilHum;
        float targetSoil;
        float soilTolerance;

        
        float pumpDuration;
        float absorptionDelay;
        
        float dayTime[2];
        float nightTime[2];

        float WaterReserv;
        float HumidReserv;

        float ReservWarning;

        bool lightStatus;
        bool pumpStatus;
        bool coolerStatus;
        bool heaterStatus;
        bool humidStatus;
        bool dehumidStatus;

        Light *light;

    public:

        DataClass(Light *light);

        void setTemp(float temp);
        void setTargetTemp(float target_temp);
        void setTempTolerance(float temp_tol);

        void setHumid(float humid);
        void setTargetHumid(float target_hum);
        void setHumidTolerance(float hum_tol);

        void setSoil(float soil);
        void setTargetSoil(float target_soil);
        void setSoilTolerance(float soil_tol);

        void setPumpDuration(float pump_duration);
        void setAbsorptionDelay(float abs_delay);

        void setDayTime(float h, float m);
        void setNightTime(float h, float m);

        void setWaterRes(float value);
        void setHumidRes(float value);
        void setReservWarning(float value);

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
        float getTargetSoil();
        float getSoilTolerance();
        float getPumpDuration();
        float getAbsorptionDelay();


        void getDayTime(float *out);
        void getNightTime(float *out);

        float getWaterRes();
        float getHumidRes();
        float getReservWarning();

        bool getLightStatus();
        bool getPumpStatus();
        bool getCoolerStatus();
        bool getHeaterStatus();
        bool getHumidStatus();
        bool getDehumidStatus();

        void setIsRunning(bool stats);
        bool getIsRunning();

       
};