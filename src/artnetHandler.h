#ifndef ARTNET_HANDLER_H
#define ARTNET_HANDLER_H

#include <Artnet.h>
#include "config.h"
#include "logger.h"
#include "device/device.h"

class ArtnetHandler
{
private:
    ArtnetWiFi artnet;
    uint8_t prevDmx[512];
    bool isFirst = true;
    Device **devices;
    uint8_t devicesCount;

public:
    ArtnetHandler();
    void init(int universe, const String &shortName, const String &longName, Device **dmxDevice, uint8_t devicesCount);
    void loop();
    void stop();
    uint8_t getData(uint8_t index);
};

ArtnetHandler::ArtnetHandler(){};

void ArtnetHandler::init(int universe, const String &shortName, const String &longName, Device **dmxDevice, uint8_t devicesCount)
{
    artnet.shortname(shortName);
    artnet.longname(longName);
    for (size_t i = 0; i < 512; i++)
    {
        prevDmx[i] = 0;
    }
    devices = dmxDevice;
    this->devicesCount = devicesCount;
    for (uint8_t i = 0; i < devicesCount; i++)
    {
        if (dmxDevice[i])
            dmxDevice[i]->start();
    }

    artnet.begin();
    artnet.subscribe([&](const uint32_t univ, const uint8_t *data, const uint16_t size) {
        //Serial.write("-");
        if (artnet.opcode() == arx::artnet::OPC(arx::artnet::OpCode::Dmx) && size >= 2)
        {
            for (uint8_t k = 0; k < this->devicesCount; k++)
                if (devices[k])
                    devices[k]->frame(univ, data, size);
            if (isFirst || memcmp(prevDmx, data, size) != 0)
            {
                for (uint8_t i = 0; i < size; i++)
                {
                    if (prevDmx[i] != data[i])
                    {
                        prevDmx[i] = data[i];

                        for (uint8_t k = 0; k < this->devicesCount; k++)
                            if (devices[k] && devices[k]->getChannel() <= i + 1 && devices[k]->getChannel() + devices[k]->getNumberOfChannels() > i + 1)
                            {
                                devices[k]->set(i + 1, data[i]);
                            }
                        //LOG("DMX[" + String(i + 1) + "]=" + String(data[i]));
                    }
                }
                isFirst = false;
            }
        }
    });
}

uint8_t ArtnetHandler::getData(uint8_t index)
{
    if (index < 512)
        return prevDmx[index];
}

void ArtnetHandler::loop()
{
    artnet.parse();
}

void ArtnetHandler::stop()
{
    artnet.unsubscribe();
}

#endif