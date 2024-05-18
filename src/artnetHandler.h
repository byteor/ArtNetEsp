#ifndef ARTNET_HANDLER_H
#define ARTNET_HANDLER_H

#include <ArtnetWiFi.h>
#include "config.h"
#include "hw/logger.h"
#include "device/device.h"

class ArtnetHandler
{
private:
    ArtnetWiFiReceiver artnet;
    bool isFirst = true;
    Device **devices;
    uint8_t devicesCount;
    int universe;

public:
    ArtnetHandler();
    void init(int universe, const String &shortName, const String &longName, Device **dmxDevice, uint8_t devicesCount);
    void loop();
    void stop();
};

ArtnetHandler::ArtnetHandler(){};

void ArtnetHandler::init(int universe, const String &shortName, const String &longName, Device **dmxDevice, uint8_t devicesCount)
{
    this->universe = universe;
    devices = dmxDevice;
    this->devicesCount = devicesCount;
    for (uint8_t i = 0; i < devicesCount; i++)
        if (dmxDevice[i])
            dmxDevice[i]->start();

    artnet.setArtPollReplyConfig(0xFF, // OemUnknown https://github.com/tobiasebsen/ArtNode/blob/master/src/Art-NetOemCodes.h
                                 0x00, // ESTA manufacturer code
                                 0x00, // Unknown / Normal
                                 0x08, // sACN capable
                                 shortName, longName, "");

    artnet.begin();

    artnet.subscribeArtDmxUniverse(universe, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                   {
                                       if (size >= 2)
                                       {
                                           for (uint8_t k = 0; k < this->devicesCount; k++)
                                           {
                                               if (devices[k])
                                               {
                                                   devices[k]->frame(universe, data, size);

                                                   for (int i = devices[k]->getChannel(); i < devices[k]->getChannel() + devices[k]->getNumberOfChannels(); i++)
                                                   {
                                                       if (data[i - 1] != devices[k]->get(i))
                                                       {
                                                           devices[k]->set(i, data[i - 1]);
                                                       }
                                                   }
                                               }
                                           }
                                       } });
}
void ArtnetHandler::loop()
{
    artnet.parse();
}

void ArtnetHandler::stop()
{
    artnet.unsubscribeArtDmxUniverse(this->universe);
}

#endif