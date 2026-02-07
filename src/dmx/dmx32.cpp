#ifdef ESP32

#include "dmx.h"
#include "../hw/board.h"

int transmitPin = DMX_TX_PIN;
int receivePin = DMX_RX_PIN;
int enablePin = DMX_ENABLE_PIN;
dmx_port_t dmxPort = DMX_PORT;

void DmxProxy::senderTaskThunk(void *param)
{
    DmxProxy *self = static_cast<DmxProxy *>(param);
    self->senderTaskLoop();
}

void DmxProxy::senderTaskLoop()
{
    uint8_t localBuf[DMX_CHANNELS];
    memset(localBuf, 0, DMX_CHANNELS);
    const TickType_t delayTicks = pdMS_TO_TICKS(refreshIntervalMs);
    for (;;)
    {
        // Copy data under mutex, then release before blocking I/O
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE)
        {
            memcpy(localBuf, data, DMX_CHANNELS);
            xSemaphoreGive(dataMutex);
        }
        dmx_write(dmxPort, localBuf, DMX_CHANNELS);
        dmx_send_num(dmxPort, DMX_CHANNELS);
        dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
        vTaskDelay(delayTicks);
    }
}

void DmxProxy::init()
{
    LOG(F("ESP32 DMX writer init"));
    LOG("TX: " + String(transmitPin) + " RX: " + String(receivePin) + " EN: " + String(enablePin));
    memset(data, 0, DMX_CHANNELS); // start code (slot 0) must be 0x00
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmx_personality_t personalities[] = {};
    int personality_count = 0;
    dmx_driver_install(dmxPort, &config, personalities, personality_count);
    dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

    dataMutex = xSemaphoreCreateMutex();
    if (!senderTask)
    {
        const uint32_t stackSize = 4096;
        const UBaseType_t priority = 2;
        const BaseType_t result = xTaskCreatePinnedToCore(
            DmxProxy::senderTaskThunk,
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
}

void DmxProxy::write(int channel, uint8_t value)
{
    // Set DMX channel value
    if (channel > 0 && channel < DMX_CHANNELS)
    {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            data[channel] = value;
            xSemaphoreGive(dataMutex);
        }
    }
}

void DmxProxy::update()
{
    if (!senderTask)
    {
        dmx_write(dmxPort, data, DMX_CHANNELS);
        dmx_send_num(dmxPort, DMX_CHANNELS);
        dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
    }
}

#endif