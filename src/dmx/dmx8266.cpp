#ifdef ESP8266

#include "dmx.h"

void DmxProxy::init()
{
    LOG(F("ESP8266 DMX writer init"));
    this->dmx.init(DMX_CHANNELS - 1);
}

void DmxProxy::write(int channel, uint8_t value)
{
    // Set DMX channel value
    this->dmx.write(channel, value);
}

void DmxProxy::update()
{
    // Update DMX data
    this->dmx.update();
}

#endif