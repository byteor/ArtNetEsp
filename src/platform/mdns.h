#ifndef PLATFORM_MDNS_H
#define PLATFORM_MDNS_H

#if defined(ESP8266)
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#endif

#include "hw/logger.h"

namespace platform
{
    // Advertises the device as <hostname>.local via mDNS, with an _http._tcp
    // service record for the web UI/REST API on port 80. Call once, after
    // WiFi is connected (Connect::connect() returning means the STA path
    // succeeded - the portal path never returns, it ESP.restart()s instead).
    inline void mdnsBegin(const String &hostname)
    {
        if (MDNS.begin(hostname.c_str()))
        {
            MDNS.addService("http", "tcp", 80);
            LOG("mDNS responder started: " + hostname + ".local");
        }
        else
        {
            LOG(F("mDNS responder failed to start"));
        }
    }

    // ESP8266's mDNS responder needs polling from loop(); ESP32's runs in its
    // own task and has no update() to call.
    inline void mdnsLoop()
    {
#if defined(ESP8266)
        MDNS.update();
#endif
    }
}

#endif
