#ifndef NET_ARTNET_SERVICE_H
#define NET_ARTNET_SERVICE_H

#include <ArtnetWiFi.h>
#include "device/device.h"

class ArtnetService
{
private:
    ArtnetWiFiReceiver artnet;
    Device **devices;
    uint8_t devicesCount;
    int universe;

public:
    ArtnetService();
    void init(int universe, const String &shortName, const String &longName, Device **dmxDevice, uint8_t devicesCount);
    void loop();
    void stop();
};

#endif
