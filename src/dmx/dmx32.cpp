#ifdef ESP32

#include "dmx.h"
#include "../hw/board.h"

int transmitPin = DMX_TX_PIN;
int receivePin = DMX_RX_PIN;
int enablePin = DMX_ENABLE_PIN;
dmx_port_t dmxPort = DMX_PORT;

void DmxProxy::init()
{
    LOG(F("ESP32 DMX writer init"));
    LOG("TX: " + String(transmitPin) + " RX: " + String(receivePin) + " EN: " + String(enablePin));
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmx_personality_t personalities[] = {};
    int personality_count = 0;
    dmx_driver_install(dmxPort, &config, personalities, personality_count);
    dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);
}

void DmxProxy::write(int channel, uint8_t value)
{
    // Set DMX channel value
    if (channel < DMX_CHANNELS)
    {
        data[channel] = value; // TODO: double check if it needs to be channel +/- 1
    }
}

void DmxProxy::update()
{
    dmx_write(dmxPort, data, DMX_CHANNELS);
    dmx_send_num(dmxPort, DMX_CHANNELS);
    dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
}

#endif