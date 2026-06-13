#include "net/artnetService.h"

ArtnetService::ArtnetService() {}

void ArtnetService::init(int universe, const String &shortName, const String &longName, Device **dmxDevice, uint8_t devicesCount)
{
    this->universe = universe;
    devices = dmxDevice;
    this->devicesCount = devicesCount;

    artnet.setArtPollReplyConfig(0xFF, // OemUnknown https://github.com/tobiasebsen/ArtNode/blob/master/src/Art-NetOemCodes.h
                                 0x00, // ESTA manufacturer code
                                 0x00, // Unknown / Normal
                                 0x08, // sACN capable
                                 shortName, longName, "");

    artnet.begin();

    artnet.subscribeArtDmxUniverse(universe, [this](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                   {
                                       if (size < 2)
                                           return;

                                       for (uint8_t k = 0; k < this->devicesCount; k++)
                                       {
                                           Device *device = devices[k];
                                           if (!device)
                                               continue;

                                           // Slice the incoming frame down to this device's
                                           // configured channel range. firstChannel() is
                                           // 1-based DMX channel numbering; `start` is the
                                           // matching 0-based offset into `data`. If
                                           // firstChannel() == 0 (misconfigured), `start`
                                           // underflows to a huge value, `start < size` is
                                           // false, and `len` stays 0 - the device is
                                           // skipped, same as before.
                                           uint16_t start = device->firstChannel() - 1;
                                           if (start >= size)
                                               continue;

                                           uint16_t available = size - start;
                                           uint16_t len = device->channelCount() < available ? device->channelCount() : available;
                                           if (len == 0)
                                               continue;

                                           device->onDmx(this->universe, data + start, len);
                                       } });
}

void ArtnetService::loop()
{
    artnet.parse();
}

void ArtnetService::stop()
{
    artnet.unsubscribeArtDmxUniverse(this->universe);
}
