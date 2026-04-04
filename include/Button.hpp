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
        unsigned long idleTimeout = 60000;
        bool isIdle;
        bool longPressTriggered = false;

        // held() — estado interno
        unsigned long heldLastTick = 0;
        bool heldActive = false;

    public:
        Button(uint8_t pin_number);
        void init();
        uint8_t getPin();
        bool read();
        void idleButton();
        bool getIdle(unsigned long timeout = 60000);
        bool longPress(unsigned long holdTime = 3000);

        // Retorna true a cada 'interval' ms enquanto o botão estiver pressionado.
        // Ignora os primeiros 'delay' ms para não disparar no clique normal.
        bool held(unsigned long interval = 250, unsigned long delay = 500);
};