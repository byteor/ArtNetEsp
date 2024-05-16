#include "strobe.h"

Strobe::Strobe(uint8_t universe, uint8_t channel, uint8_t pin, int pulse, int multiplier, int activeState)
{
    Serial.printf("New Strobe: pin=%d, DMX channel:%d\r\n", pin, channel);

    this->universe = universe;
    this->channel = channel;
    this->pin = pin;
    this->pulse = pulse;
    this->multiplier = multiplier;
    enabled = true;
    this->activeState = activeState;
    this->inactiveState = activeState == HIGH ? LOW : HIGH;
    this->state = activeState;
    this->valueOverride = activeState == HIGH ? 0 : 255;
    this->adjustedMaxValue = activeState == HIGH ? 255 : 0;

    interval = 0;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, inactiveState);
}

void Strobe::set(uint8_t dmxChannel, uint8_t data)
{
    if (dmxChannel == channel) // Dimmer
    {
        Serial.println("Dimmer: " + String(dmxChannel) + " = " + String(data));
        this->value = data;
        this->adjustedActiveValue = activeState == HIGH ? value : 255 - value;
        this->adjustedInactiveValue = activeState == HIGH ? 0 : 255;
        update();
    }
    else if (dmxChannel == channel - 1) // Strobe
    {
        Serial.println("Dimmer Strobe: " + String(dmxChannel) + " = " + String(data));
        setDuration(pulse);
        setInterval(data * multiplier);
    }
}

void Strobe::update()
{
    if (enabled && state == activeState)
    {
        analogWrite(pin, adjustedActiveValue * 4); // translate from 0-255 to 0-1024
        Serial.println(" =" + String(adjustedActiveValue));
    }
    else
    {
        analogWrite(pin, adjustedInactiveValue);
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
    if (!enabled)
        return;
    unsigned long currentMillis = millis();
    if (lastChange != 0 && millis() - lastChange > DMX_SILENCE_TIMEOUT)
    {
        set(channel, 0);
        lastChange = millis();
    }
    else if (period <= pulse && enabled)
    {
        previousMillis = currentMillis;
        if (state != activeState)
        {
            state = activeState;
            update();
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
            update();
        }
    }
}

void Strobe::start()
{
    if (!enabled)
    {
        Serial.println("Strobe Started");
        interval = 0;
        state = activeState;
        enabled = true;
        lastChange = millis();
    }
}

void Strobe::stop()
{
    enabled = false;
    digitalWrite(pin, inactiveState);
}

void Strobe::flip()
{
    enabled = !enabled;
    if (enabled)
    {
        Serial.println("Flip - ON");
        // flipping when DMX value is set to 0. Manual flip should bring the full light on
        valueOverride = value > 0 ? adjustedActiveValue : adjustedMaxValue;
    }
    else
    {
        Serial.println("Flip - OFF");
        valueOverride = adjustedInactiveValue;
    }
    update();
}

bool Strobe::isEnabled()
{
    return enabled;
}