#include "strobe.h"

Strobe::Strobe(uint8_t universe, uint8_t channel, uint8_t pin, int pulse, int multiplier, int activeState, bool dimmable)
{
    Serial.printf("New Strobe: pin=%d, DMX channel:%d, dimmable:%d\r\n", pin, channel, dimmable);

    this->universe = universe;
    this->channel = channel;
    this->pin = pin;
    this->pulse = pulse;
    this->multiplier = multiplier;
    enabled = false;
    this->activeState = activeState;
    this->inactiveState = activeState == HIGH ? LOW : HIGH;
    this->dimmable = dimmable;
    interval = 0;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, inactiveState);
}

void Strobe::set(uint8_t dmxChannel, uint8_t data)
{
    if (dmxChannel == channel) // Dimmer
    {
        data == 0 ? stop() : start();
        this->value = data;
        this->adjustedActiveValue = activeState == HIGH ? value : 255 - value;
        this->adjustedInactiveValue = activeState == HIGH ? 0 : 255;
        if (enabled && dimmable && state == activeState)
        {
            // if it is currently ON adjust the PWM value
            analogWrite(pin, adjustedActiveValue);
        }
    }
    if (dmxChannel == channel - 1) // Strobe
    {
        setDuration(pulse);
        setInterval(value * multiplier);
    }
}

void Strobe::setInterval(int millis)
{
    period = millis;
    if (period < 0)
        period = pulse;
}

void Strobe::setDuration(int millis)
{
    pulse = millis;
    if (pulse < 0)
        pulse = 0;
}

void Strobe::setPin(int number)
{
    if (pin != number)
    {
        pinMode(pin, INPUT);
        pin = number;
        pinMode(pin, OUTPUT);
        digitalWrite(pin, inactiveState);
    }
}

void Strobe::handle()
{
    unsigned long currentMillis = millis();
    if (lastChange != 0 && millis() - lastChange > DMX_SILENCE_TIMEOUT)
    {
        set(channel, 0);
    }
    else if (period <= pulse && enabled)
    {
        previousMillis = currentMillis;
        if (state != activeState)
        {
            state = activeState;
            if (dimmable)
            {
                analogWrite(pin, adjustedActiveValue);
            }
            else
            {
                digitalWrite(pin, state);
            }
        }
    }
    else
    {
        if (currentMillis - previousMillis >= interval && enabled)
        {
            previousMillis = currentMillis;
            if (state == activeState)
            {
                state = inactiveState;
                interval = period - pulse;
            }
            else
            {
                state = activeState;
                interval = pulse;
            }

            if (dimmable)
            {
                analogWrite(pin, state == activeState ? adjustedActiveValue : adjustedInactiveValue);
            }
            else
            {
                digitalWrite(pin, state);
            }
        }
    }
}

void Strobe::start()
{
    if (!enabled)
    {
        interval = 0;
        state = inactiveState;
        enabled = true;
    }
}

void Strobe::stop()
{
    enabled = false;
    digitalWrite(pin, inactiveState);
}

void Strobe::flip()
{
    // TODO: FLIP!
}