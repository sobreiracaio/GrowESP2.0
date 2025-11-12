#pragma once

#include "Libraries.hpp"

class Display;

class Button {
    
    private:

    Display **display;
    uint8_t pin;

    public:
        Button();
        void init();
};