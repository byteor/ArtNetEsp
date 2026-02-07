#pragma once

#ifndef SONOFF_BASIC

#include <Arduino.h>

#ifdef ESP32
#include <esp_dmx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
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
    SemaphoreHandle_t dataMutex = nullptr;
    TaskHandle_t senderTask = nullptr;
    uint32_t refreshIntervalMs = 40;
    static void senderTaskThunk(void *param);
    void senderTaskLoop();
#else
    DMXESPSerial dmx;
#endif

public:
    void init();
    void write(int chanel, uint8_t value);
    void update();
};

#endif
