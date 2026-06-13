#include "../boards/features.h"

#if FEATURE_DMX_PORT

#ifndef REPEATER_H
#define REPEATER_H

#include <Arduino.h>
#include "../dmx/dmx.h"
#include "device.h"

#define DMX_REFRESH_INTERVAL 40

class DmxRepeater : public Device
{
protected:
    uint8_t pin;
    // DmxPort::instance() (B15) - a reference, not a value member, so
    // multiple Repeater devices share the one hardware DMX port/task.
    DmxPort &dmx;
    unsigned long lastRefreshTime = 0;

public:
    DmxRepeater(uint8_t universe);
    void start() override;
    void tick() override;
    void frame(const uint32_t univ, const uint8_t *data, const uint16_t size) override;
    uint16_t channelCount() override { return DMX_CHANNELS; }
};

DmxRepeater::DmxRepeater(uint8_t universe) : dmx(DmxPort::instance())
{
    Serial.printf("New Repeater\r\n");
    dmx.init();
    this->universe = universe;
    // firstChannel() must be >= 1 so ArtnetService's slice arithmetic
    // (start = firstChannel() - 1) doesn't underflow; the repeater consumes
    // the whole frame from channel 1 regardless (channelCount() == DMX_CHANNELS).
    channel = 1;
}

void DmxRepeater::start()
{
}

void DmxRepeater::frame(const uint32_t univ, const uint8_t *data, const uint16_t size)
{
    dmx.writeFrame(data, size);
    Device::frame();
}

void DmxRepeater::tick()
{
    if (millis() - lastRefreshTime >= DMX_REFRESH_INTERVAL)
    {
        lastRefreshTime += DMX_REFRESH_INTERVAL;
        dmx.update();
    }
}

#endif // REPEATER_H
#endif // FEATURE_DMX_PORT
