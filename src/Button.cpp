/*****************************************************************************************************/
// ░▒▓██████▓▒░░▒▓███████▓▒░ ░▒▓██████▓▒░░▒▓█▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓███████▓▒░ ░▒▓██████▓▒░░▒▓█▓▒░░▒▓█▓▒░//
//░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░// 
//░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░// 
//░▒▓█▓▒▒▓███▓▒░▒▓███████▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓███████▓▒░░▒▓█▓▒░░▒▓█▓▒░░▒▓██████▓▒░ // 
//░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░// 
//░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░// 
// ░▒▓██████▓▒░░▒▓█▓▒░░▒▓█▓▒░░▒▓██████▓▒░ ░▒▓█████████████▓▒░░▒▓███████▓▒░ ░▒▓██████▓▒░░▒▓█▓▒░░▒▓█▓▒░//
//                                                                                                   //
//                                                                                                   //
// Author: Caio Sobreira                                                 Version: 1.0                //
/*****************************************************************************************************/

#include "Button.hpp"

Button::Button(uint8_t pin_number) : pin(pin_number), lastReading(false), stableState(false),
    lastDebounceTime(0), pressStartTime(0), isIdle(false) 
    {
        lastActivity = millis();
        
    }


void Button::init()
{
    pinMode(pin, INPUT);
    
}

uint8_t Button::getPin()
{
    return pin;
}

bool Button::read()
{
    const unsigned long debounceDelay = 50;

    bool reading = !digitalRead(pin);
    unsigned long now = millis();

    if (reading != lastReading)
        lastDebounceTime = now;

    if ((now - lastDebounceTime) > debounceDelay) {
        if (reading != stableState) {
            stableState = reading;

            if (stableState) {
                lastReading = reading;
                idleButton(); // reseta timer automaticamente a cada pressão
                return true;
            }
        }
    }

    lastReading = reading;
    return false;
}



void Button::idleButton()
{
    lastActivity = millis();
    isIdle = false;
}

bool Button::getIdle(unsigned long timeout)
{
    unsigned long now = millis();
    
    // Se não teve atividade por tempo suficiente → idle
    if (!isIdle && (now - lastActivity > timeout)) {
        isIdle = true;
    }

    return isIdle;
}

bool Button::longPress(unsigned long holdTime)
{
    bool reading = !digitalRead(pin);

    if (reading) {
        if (millis() - lastDebounceTime > holdTime && !longPressTriggered) {
            longPressTriggered = true;
            idleButton();
            return true;
        }
    } else {
        longPressTriggered = false;
    }

    return false;
}