#pragma once

#include <Arduino.h>

#ifdef ESP32
#include <esp_dmx.h>
#else
#include <ESPDMX.h>
#endif
#include "../hw/logger.h"

#define DMX_CHANNELS 512

class DmxProxy
{
protected:
#ifdef ESP32
    uint8_t data[DMX_CHANNELS];
#else
    DMXESPSerial dmx;
#endif

public:
    void init();
    void write(int chanel, uint8_t value);
    void update();
};
