#ifndef REPEATER_H
#define REPEATER_H

#include <Arduino.h>
#include <ESPDMX.h>
#include "device.h"

#define DMX_REFRESH_INTERVAL 40
#define DMX_CHANNELS 512

class DmxRepeater : public Device
{
protected:
    uint8_t pin;
    DMXESPSerial dmx;
    unsigned long lastRefreshTime = 0;

public:
    DmxRepeater(uint8_t universe);
    void start();
    void handle();
    void frame(const uint32_t univ, const uint8_t *data, const uint16_t size);
    uint16_t getNumberOfChannels() { return DMX_CHANNELS; }
};

DmxRepeater::DmxRepeater(uint8_t universe)
{
    Serial.printf("New Repeater\r\n");
    dmx.init(DMX_CHANNELS - 1);
    this->universe = universe;
    channel = 0;
}

void DmxRepeater::start()
{
}

void DmxRepeater::frame(const uint32_t univ, const uint8_t *data, const uint16_t size)
{
    for (uint16_t i = 0; i < size; i++)
    {
        dmx.write(i + 1, data[i]);
    }
    Device::frame();
}

void DmxRepeater::handle()
{
    if (millis() - lastRefreshTime >= DMX_REFRESH_INTERVAL)
    {
        lastRefreshTime += DMX_REFRESH_INTERVAL;
        dmx.update();
    }
}

#endif