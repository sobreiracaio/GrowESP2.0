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
                idleButton();
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

// ── held ──────────────────────────────────────────────────────────────────────
// Retorna true a cada 'interval' ms enquanto o botão estiver pressionado.
// Espera 'delay' ms antes de começar a disparar — evita conflito com read().
// Exemplo: btn[0]->held() → true a cada 250ms após 500ms pressionado
bool Button::held(unsigned long interval, unsigned long delay)
{
    bool pressing = !digitalRead(pin);
    unsigned long now = millis();

    if (!pressing) {
        // Botão solto — reseta estado
        heldActive = false;
        heldLastTick = 0;
        return false;
    }

    // Botão pressionado — inicializa na primeira detecção
    if (!heldActive) {
        heldActive = true;
        heldLastTick = now + delay; // espera 'delay' antes do primeiro tick
        return false;
    }

    // Dispara a cada 'interval' ms após o delay inicial
    if (now >= heldLastTick) {
        heldLastTick = now + interval;
        idleButton(); // mantém tela acesa enquanto segura
        return true;
    }

    return false;
}