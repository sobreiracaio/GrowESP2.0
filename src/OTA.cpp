#include "OTA.hpp"

OTA::OTA(Preferences *prefs) : prefs(prefs)
{
    serialNum = 0;
    digit1 = 0;
    digit2 = 0;
    digit3 = 0;
    
    //firmwareUrl = "";
}

void OTA::setSerialNum()
{
    int res = (digit1 * 100) + (digit2 * 10) + digit3;
    serialNum = res;
}

int OTA::getSerialNum()
{   
    return serialNum;
}

void OTA::setDigit(int position, int value)
{
    switch (position)
    {
    case 1:
        digit1 = value;
        return;
    case 2:
        digit2 = value;
        return;
    case 3:
        digit3 = value;
        return;
    default:
        return;
    }
}

int OTA::getDigit(int position)
{
    switch (position)
    {
    case 1:
        return digit1;
    case 2:
        return digit2;
    case 3:
        return digit3;
    default:
        return -1;
    }
}