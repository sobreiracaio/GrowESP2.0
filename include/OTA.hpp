#pragma once

#include "Libraries.hpp"

class Display;

class OTA {
    private:

    Display *display;

    public:
        OTA(Display *tft);
};