#ifdef ESP8266
#ifndef SONOFF_BASIC

#include "dmx.h"

void DmxProxy::init()
{
    // instance() guarantees one DmxProxy, but init() is still called
    // once per DmxRepeater constructed - make it idempotent (B15).
    if (initialized)
        return;
    initialized = true;

    LOG(F("ESP8266 DMX writer init"));
    // ESPDMX's internal buffer is a fixed uint8_t[512] (start code +
    // 511 channel slots): chanSize=512 lets update() transmit
    // dmxData[0..511] (start code + channels 1-511), but write(512,
    // ...) would index dmxData[512], one past the array, corrupting
    // adjacent memory. DmxProxy::write() below rejects channel 512
    // for exactly that reason - DMX channel 512 (B5) can't be reached
    // on ESP8266 without patching the vendored ESPDMX library.
    this->dmx.init(DMX_CHANNELS);
}

void DmxProxy::write(int channel, uint8_t value)
{
    // Set DMX channel value (1-511 - see init())
    if (channel > 0 && channel < DMX_CHANNELS)
    {
        this->dmx.write(channel, value);
    }
}

void DmxProxy::writeFrame(const uint8_t *frameData, uint16_t size)
{
    if (size > DMX_CHANNELS)
        size = DMX_CHANNELS;
    for (uint16_t i = 0; i < size; i++)
        write(i + 1, frameData[i]);
}

void DmxProxy::update()
{
    // Update DMX data
    this->dmx.update();
}

#endif
#endif