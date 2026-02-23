#pragma once

#include "Libraries.hpp"

#define DECREMENT 0
#define INCREMENT 1
#define CHANGE_STATE 2

class Button{
    
    private:
        uint8_t pin;
        bool lastReading;
        bool stableState;
        unsigned long lastDebounceTime;
        unsigned long pressStartTime;

        unsigned long lastActivity = 0;
        unsigned long idleTimeout = 60000; // 30s por exemplo
        bool isIdle;
        bool longPressTriggered = false;

    public:
        Button(uint8_t pin_number);
        void init();
        uint8_t getPin();
        bool read();
        void idleButton();
        bool getIdle(unsigned long timeout = 60000);
        bool longPress(unsigned long holdTime = 3000);
};