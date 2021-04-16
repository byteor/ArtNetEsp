#ifdef ESP32
#include <AsyncUDP.h>
#endif

#ifdef ESP8266
#include <ESPAsyncUDP.h>
extern "C"
{
#include "user_interface.h"
}
#endif

typedef struct
{
    char artNet[8];     // 'Art-Net'
    uint16_t OpCode;  // See Doc. Table 1 - OpCodes eg. 0x5000 OpOutput / OpDmx
    uint16_t version; // 0x0e00 (aka 14)
    uint8_t seq;      // monotonic counter
    uint8_t physical; // 0x00
    uint16_t net;     // universe
    uint16_t length;  // data length (2 - 512) [network byte order]
    uint8_t data[512];
} ArtnetDmxFull;

typedef std::function<void(int chanel, int value)> DmxCallbackFunction;

class UdpHandler
{
    protected:
    AsyncUDP *udp;
    Stream *serial;
    bool isFirst = true;

    DmxCallbackFunction handler;

    public:
    UdpHandler(Stream *serial);
    bool start(int port, DmxCallbackFunction fn);
    void handle(AsyncUDPPacket packet, uint8_t prevDmx[]);
};