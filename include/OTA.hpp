#pragma once

#include "Libraries.hpp"




class OTA {
    private:
        int serialNum;
        int digit1;
        int digit2;
        int digit3;
        Preferences *prefs;
    

    public:
        OTA(Preferences *prefs);

        void setSerialNum();
        int getSerialNum();
        void setDigit(int position, int value);
        int getDigit(int position);

        int updateDevice();
};