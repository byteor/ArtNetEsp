#ifndef ARTNET_HANDLER_H
#define ARTNET_HANDLER_H

#include <ArtnetWiFi.h>
#include "config.h"
#include "hw/logger.h"
#include "device/device.h"
#include "core/dmxTypes.h"

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

ArtnetHandler::ArtnetHandler() {};

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

    artnet.subscribeArtDmxUniverse(universe, [this](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                   {
                                       if (size >= 2)
                                       {
                                           for (uint8_t k = 0; k < this->devicesCount; k++)
                                           {
                                               if (devices[k])
                                               {
                                                   devices[k]->frame(this->universe, data, size);

                                                   // Frame-consuming devices (e.g. the Repeater, whose frame()
                                                   // already copied the whole packet) report DMX_CHANNELS
                                                   // channels - skip the per-channel set()/get() dispatch
                                                   // for them. core::DMX_CHANNELS (src/core/dmxTypes.h) is
                                                   // platform-free, includable even when FEATURE_DMX_PORT=0
                                                   // (AGENTS.md Gotcha #4).
                                                   if (devices[k]->channelCount() == core::DMX_CHANNELS)
                                                       continue;

                                                   for (int i = devices[k]->getChannel(); i < devices[k]->getChannel() + devices[k]->channelCount(); i++)
                                                   {
                                                       if (i < 1)
                                                           continue;

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