#ifndef RELAY_H
#define RELAY_H

#include <Arduino.h>
#include "device.h"
#include "../hw/logger.h"

class DmxRelay : public Device
{
protected:
    uint8_t pin;
    uint8_t active_value;
    uint8_t inactive_value;
    uint8_t threshold;
    uint8_t value;
    uint8_t data;

public:
    void start() override;
    void flip() override;
    uint8_t get(uint16_t channel) override;
    void set(uint16_t channel, uint8_t data) override;
    uint16_t channelCount() override { return 1; }
    bool isEnabled() override;

    DmxRelay(uint8_t universe, uint16_t channel, uint8_t pin, uint8_t active_value, uint8_t threshold);
};

DmxRelay::DmxRelay(uint8_t universe, uint16_t channel, uint8_t pin, uint8_t active_value, uint8_t threshold)
{
    this->universe = universe;
    this->channel = channel;
    this->pin = pin;
    this->active_value = active_value == LOW ? LOW : HIGH;
    this->inactive_value = active_value == LOW ? HIGH : LOW;
    this->value = this->inactive_value;
    this->threshold = threshold;
    Serial.print("Relay on pin: ");
    Serial.print(pin);
    Serial.print(" DMX channel: ");
    Serial.print(channel);
    Serial.print(" Universe: ");
    Serial.println(universe);
}

uint8_t DmxRelay::get(uint16_t channel)
{
    return data;
}

void DmxRelay::set(uint16_t dmxChannel, uint8_t data)
{
    if (dmxChannel == channel)
    {
        this->data = data;
        value = data > threshold ? active_value : inactive_value;
        LOG_DEBUG("Relay: " + String(value));
        digitalWrite(pin, value);
    }
}

void DmxRelay::start()
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
}

void DmxRelay::flip()
{
    value = !value;
    manualOverride = true; // B20: don't let silence-blackout undo this until the next DMX frame
    Serial.print("Relay: FLIP: ");
    Serial.println(value & 0x01);
    digitalWrite(pin, value);
}

bool DmxRelay::isEnabled()
{
    return value == active_value;
}

#endif // RELAY_H