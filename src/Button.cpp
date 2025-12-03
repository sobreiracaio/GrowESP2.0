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

bool Button::read(float *value, int operation, float upper_limit, float lower_limit, unsigned long holdTime, float step)
{
    const unsigned long debounceDelay = 50;
    bool currentReading = !digitalRead(pin);
    unsigned long now = millis();

    // Debounce
    if (currentReading != lastReading) {
        lastDebounceTime = now;
    }

    if ((now - lastDebounceTime) > debounceDelay) {
        if (currentReading != stableState) {
            stableState = currentReading;

            // DETECÇÃO DA BORDA DE SUBIDA (botão pressionado agora)
            if (stableState) 
            {
                pressStartTime = now;

                idleButton();
                
                // Se não há tempo mínimo, executa imediatamente
                if (holdTime == 0) {
                    if (operation == DECREMENT) {
                        (*value) -= step;
                        if (*value <= lower_limit - 1) *value = upper_limit;
                        
                    }
                    else if (operation == INCREMENT) {
                        (*value) += step;
                        if (*value >= upper_limit + 1) *value = lower_limit;
                    }
                    else if (operation == CHANGE_STATE) {
                        *value = !(*value);
                    }
                    else if (operation == -1)
                    {
                        return true;
                    }
                    return true;
                }
            }
        }
    }

    lastReading = currentReading;

    // Com tempo mínimo: verifica se holdTime foi atingido
    if (stableState && holdTime > 0 && (now - pressStartTime >= holdTime)) {
        pressStartTime = now + 999999; // evita múltiplos disparos

                
        if (operation == DECREMENT) 
        {
            (*value) -= step;
            if (*value <= lower_limit - 1) *value = upper_limit;
        }

        else if (operation == INCREMENT) 
        {
            (*value) += step;
            if (*value >= upper_limit + 1) *value = lower_limit;
        }
        else if (operation == CHANGE_STATE) {
            *value = !(*value);
        }
        else if (operation == -1)
        {
            return true;
        }
        return true;
    }

    return false;
}


bool Button::getState()
{
    return (digitalRead(pin));
}

void Button::idleButton()
{
    lastActivity = millis();
    isIdle = false;
}

bool Button::getIdle()
{
    unsigned long now = millis();
    
    // Se não teve atividade por tempo suficiente → idle
    if (!isIdle && (now - lastActivity > idleTimeout)) {
        isIdle = true;
    }

    return isIdle;
}