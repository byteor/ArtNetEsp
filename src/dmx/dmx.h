#pragma once

#include "../boards/features.h"

#if FEATURE_DMX_PORT

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

// A DMX512 packet is the start code (slot 0) followed by the 512
// channel slots (1-512) - 513 bytes on the wire (B5). DMX_CHANNELS
// above is the channel count, used for getNumberOfChannels()/range
// checks; DMX_BUFFER_SIZE is the buffer/wire size.
#define DMX_BUFFER_SIZE (DMX_CHANNELS + 1)

class DmxPort
{
protected:
#ifdef ESP32
    uint8_t data[DMX_BUFFER_SIZE];
    SemaphoreHandle_t dataMutex = nullptr;
    TaskHandle_t senderTask = nullptr;
    uint32_t refreshIntervalMs = 40;
    static void senderTaskThunk(void *param);
    void senderTaskLoop();
#else
    DMXESPSerial dmx;
#endif
    bool initialized = false;

public:
    // DmxPort wraps a singleton hardware UART (B15) - always go
    // through instance() rather than constructing a DmxPort
    // directly, so multiple Repeater devices share one port/task.
    static DmxPort &instance()
    {
        static DmxPort port;
        return port;
    }

    void init();
    void write(int channel, uint8_t value);
    // Bulk-set channels 1..size from frameData[0..size-1] in one
    // locked operation (B14) - preferred over calling write() in a
    // per-channel loop.
    void writeFrame(const uint8_t *frameData, uint16_t size);
    void update();
};

#endif // FEATURE_DMX_PORT
