#include <Arduino.h>
//#include <AsyncUDP.h>
#include "UdpHandler.h"
#include "lwip/def.h"

uint8_t prevDmx[512];

UdpHandler::UdpHandler(Stream *stream)
{
    if (!stream)
        serial = &Serial;
    else
        serial = stream;
    udp = new AsyncUDP();
}

bool UdpHandler::start(int port, Device **devices, uint8_t deviceCount)
{
    this->devices = devices;
    this->devicesCount = deviceCount;
    serial->print("UDP connecting...");
    if (udp->connected())
        udp->close();
    if (udp->listen(port))
    {
        memset(prevDmx, 0, 512 * sizeof(uint8_t));
        serial->print("UDP is listening on port ");
        serial->println(port);
        udp->onPacket([&](AsyncUDPPacket packet) {
            this->handle(packet, prevDmx);
        });
        return true;
    }
    else
    {
        serial->println("UDP error");
    }
    return false;
}

void UdpHandler::handle(AsyncUDPPacket packet, uint8_t prevDmx[])
{
    ArtnetDmxFull *artnet = (ArtnetDmxFull *)packet.data();

    if (packet.length() <= sizeof(ArtnetDmxFull))
    {
        if (artnet->OpCode == 0x5000 && artnet->length >= 2)
        {
            // sume controllers can send wrong number of channels so limit it to what we've got
            artnet->length = min((int)ntohs(artnet->length), (int)packet.length() - 18);

            for (uint8_t k = 0; k < this->devicesCount; k++)
                if (devices[k])
                    devices[k]->frame(0, artnet->data, artnet->length); // TODO: Universe!

            if (isFirst || memcmp(prevDmx, artnet->data, artnet->length) != 0)
            {
                bool changed = false;
                for (int i = 0; i < artnet->length; i++)
                {
                    if (prevDmx[i] != artnet->data[i])
                    {
                        changed = true;
                        prevDmx[i] = artnet->data[i];

                        for (uint8_t k = 0; k < this->devicesCount; k++)
                            if (devices[k] && devices[k]->getChannel() <= i + 1 && devices[k]->getChannel() + devices[k]->getNumberOfChannels() > i + 1)
                            {
                                devices[k]->set(i + 1, artnet->data[i]);
                            }

                        serial->print(" DMX: ");
                        serial->print(i);
                        serial->print(" = ");
                        serial->print(artnet->data[i]);
                    }
                }
                if (changed)
                    serial->println();
                isFirst = false;
            }
        }
        else if (artnet->OpCode == 0x2000)
        {
            serial->println("ArtNet Poll is not supported yet");
        }
        else
        {
            serial->print("From: ");
            serial->print(packet.remoteIP());
            serial->print(" Data: ");
            for (int i = 0; i < packet.length(); i++)
            {
                serial->print(packet.data()[i], HEX);
                serial->print(" ");
            }
            serial->println();
        }
    }
    else
    {
        serial->println("weird stuff");
    }
}