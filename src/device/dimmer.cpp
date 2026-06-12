#include "dimmer.h"

PwmDimmer::PwmDimmer(uint8_t universe, uint16_t channel, uint8_t pin, int pulse, int multiplier, int activeState)
{
    LOG("New Dimmer: pin=" + String(pin) + " DMX channel:" + String(channel));

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

uint8_t PwmDimmer::get(uint16_t channel)
{
    return value;
}

void PwmDimmer::set(uint16_t dmxChannel, uint8_t data)
{
    if (dmxChannel == channel) // Dimmer
    {
        LOG_DEBUG("Dimmer: " + String(dmxChannel) + " = " + String(data));
        this->value = data;
        this->adjustedActiveValue = activeState == HIGH ? value : 255 - value;
        this->adjustedInactiveValue = activeState == HIGH ? 0 : 255;
        this->valueOverride = adjustedActiveValue;
        update();
    }
    else if (dmxChannel == channel - 1) // Strobe
    {
        LOG_DEBUG("Dimmer Strobe: " + String(dmxChannel) + " = " + String(data));
        setDuration(pulse);
        setInterval(data * multiplier);
    }
}

void PwmDimmer::update()
{
    if (enabled && state == activeState)
    {
        analogWrite(pin, valueOverride);
        LOG(" =" + String(valueOverride));
    }
    else
    {
        analogWrite(pin, adjustedInactiveValue);
    }
}

void PwmDimmer::setInterval(int millis)
{
    period = millis;
    if (period < 0)
        period = pulse;
}

void PwmDimmer::setDuration(int millis)
{
    pulse = millis;
    if (pulse < 0)
        pulse = 0;
}

void PwmDimmer::setPin(int number)
{
    if (pin != number)
    {
        pinMode(pin, INPUT);
        pin = number;
        pinMode(pin, OUTPUT);
        digitalWrite(pin, inactiveState);
    }
}

void PwmDimmer::handle()
{
    if (!enabled)
        return;
    unsigned long currentMillis = millis();
    if (lastChange != 0 && millis() - lastChange > DMX_SILENCE_TIMEOUT)
    {
        LOG(F("DMX Silence Timeout"));
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

void PwmDimmer::start()
{
    if (!enabled)
    {
        LOG(F("Dimmer Started"));
        interval = 0;
        state = activeState;
        enabled = true;
        lastChange = millis();
    }
}

void PwmDimmer::stop()
{
    enabled = false;
    digitalWrite(pin, inactiveState);
}

void PwmDimmer::flip()
{
    enabled = !enabled;
    if (enabled)
    {
        LOG(F("Flip - ON"));
        // flipping when DMX value is set to 0. Manual flip should bring the full light on
        valueOverride = value > 0 ? adjustedActiveValue : adjustedMaxValue;
    }
    else
    {
        LOG(F("Flip - OFF"));
        valueOverride = adjustedInactiveValue;
    }
    update();
}

bool PwmDimmer::isEnabled()
{
    return enabled;
}