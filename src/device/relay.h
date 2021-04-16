#ifndef RELAY_H
#define RELAY_H

#include <Arduino.h>
#include "device.h"

class DmxRelay : public Device
{
protected:
    uint8_t pin;
    uint8_t active_value;
    uint8_t inactive_value;
    uint8_t threshold;
    uint8_t value;

public:
    void start();
    void flip();
    void set(uint8_t channel, uint8_t data);
    uint16_t getNumberOfChannels() { return 1; }

    DmxRelay(uint8_t universe, uint8_t channel, uint8_t pin, uint8_t active_value, uint8_t threshold);
};

DmxRelay::DmxRelay(uint8_t universe, uint8_t channel, uint8_t pin, uint8_t active_value, uint8_t threshold)
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

void DmxRelay::set(uint8_t dmxChannel, uint8_t data)
{
    if (dmxChannel == channel)
    {
        value = data > threshold ? active_value : inactive_value;
        Serial.print("Relay: ");
        Serial.println(value);
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
    Serial.print("Relay: FLIP: ");
    Serial.println(value & 0x01);
    digitalWrite(pin, value);
}

#endif // RELAY_H