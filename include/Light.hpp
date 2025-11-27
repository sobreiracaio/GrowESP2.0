#pragma once

#include "Predefinitions.hpp"
#include "Libraries.hpp"

extern struct tm now;

class Light {

    private:
        struct tm dayTime;
        struct tm nightTime;
        void (*getNowFunc)();
        bool status;

    public:
        Light();
        void setTimeFunction(void (*func)());

        void setDay(int h, int m);
        void setNight(int h, int m);
        struct tm getDay();
        struct tm getNight();

        bool getStatus();

        void run(bool isRunning);



};