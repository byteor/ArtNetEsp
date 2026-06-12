#ifdef ESP32

#include "dmx.h"
#include "../boards/board.h"

int transmitPin = DMX_TX_PIN;
int receivePin = DMX_RX_PIN;
int enablePin = DMX_ENABLE_PIN;
dmx_port_t dmxPort = DMX_PORT;

void DmxPort::senderTaskThunk(void *param)
{
    DmxPort *self = static_cast<DmxPort *>(param);
    self->senderTaskLoop();
}

void DmxPort::senderTaskLoop()
{
    uint8_t localBuf[DMX_BUFFER_SIZE];
    memset(localBuf, 0, DMX_BUFFER_SIZE);
    const TickType_t delayTicks = pdMS_TO_TICKS(refreshIntervalMs);
    for (;;)
    {
        // Copy data under mutex, then release before blocking I/O
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE)
        {
            memcpy(localBuf, data, DMX_BUFFER_SIZE);
            xSemaphoreGive(dataMutex);
        }
        dmx_write(dmxPort, localBuf, DMX_BUFFER_SIZE);
        dmx_send_num(dmxPort, DMX_BUFFER_SIZE);
        dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
        vTaskDelay(delayTicks);
    }
}

void DmxPort::init()
{
    // instance() guarantees one DmxPort, but init() is still called
    // once per DmxRepeater constructed - make it idempotent so a
    // second Repeater config doesn't re-install the driver / spawn a
    // second sender task on the same UART (B15).
    if (initialized)
        return;
    initialized = true;

    LOG(F("ESP32 DMX writer init"));
    LOG("TX: " + String(transmitPin) + " RX: " + String(receivePin) + " EN: " + String(enablePin));
    memset(data, 0, DMX_BUFFER_SIZE); // start code (slot 0) must be 0x00
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmx_personality_t personalities[] = {};
    int personality_count = 0;
    dmx_driver_install(dmxPort, &config, personalities, personality_count);
    dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

    dataMutex = xSemaphoreCreateMutex();
    const uint32_t stackSize = 4096;
    const UBaseType_t priority = 2;
    const BaseType_t result = xTaskCreatePinnedToCore(
        DmxPort::senderTaskThunk,
        "DmxSender",
        stackSize,
        this,
        priority,
        &senderTask,
        1);
    if (result == pdPASS)
    {
        LOG(F("ESP32 DMX sender task started on core 1"));
    }
    else
    {
        LOG(F("ESP32 DMX sender task start failed"));
    }
}

void DmxPort::write(int channel, uint8_t value)
{
    // Set DMX channel value (1-512; data[0] is the start code - B5)
    if (channel > 0 && channel <= DMX_CHANNELS)
    {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            data[channel] = value;
            xSemaphoreGive(dataMutex);
        }
    }
}

void DmxPort::writeFrame(const uint8_t *frameData, uint16_t size)
{
    if (size > DMX_CHANNELS)
        size = DMX_CHANNELS;
    // One memcpy + one lock for the whole frame (B14), instead of
    // size separate write() calls each taking/releasing dataMutex.
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        memcpy(&data[1], frameData, size);
        xSemaphoreGive(dataMutex);
    }
}

void DmxPort::update()
{
    if (!senderTask)
    {
        dmx_write(dmxPort, data, DMX_BUFFER_SIZE);
        dmx_send_num(dmxPort, DMX_BUFFER_SIZE);
        dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
    }
}

#endif