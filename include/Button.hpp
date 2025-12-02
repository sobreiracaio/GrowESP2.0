#pragma once

#include "Libraries.hpp"

#define DECREMENT 0
#define INCREMENT 1
#define CHANGE_STATE 2

class Button{
    
    private:
        uint8_t pin;
        uint8_t pin_mode;
        bool lastReading;
        bool stableState;
        unsigned long lastDebounceTime;
        unsigned long pressStartTime;

        unsigned long lastActivity = 0;
        const unsigned long idleTimeout = 60000; // 30s por exemplo
        bool isIdle;


    public:
        Button(uint8_t pin_number);
        void init();
        uint8_t getPin();
        bool read(float *value, int operation = -1, float upper_limit = 0, float lower_limit = 0, unsigned long holdTime = 0, float step = 1);
        bool getState();
        void idleButton();
        bool getIdle();
};