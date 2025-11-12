#include "Display.hpp"

Display::Display(TFT_eSPI **tft, FBase **fbase, Time **time, UpdateClass **ota, WifiManager **wifi_manager, Button **button) :
                    display(tft), firebase(fbase), rtc(time), update(ota), wifi(wifi_manager), btn(button) {}