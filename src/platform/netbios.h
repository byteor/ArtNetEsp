#ifndef PLATFORM_NETBIOS_H
#define PLATFORM_NETBIOS_H

// NetBIOS name responder (UDP 137). A router-independent way for LAN scanners
// to resolve <hostname> -> IP without relying on the router registering the
// DHCP hostname in its DNS. Primarily helps Windows hosts and tools that do
// NetBIOS name lookups; complements mDNS (.local) and the DHCP hostname.
//
// Both Arduino cores bundle a responder exposing the same `NBNS` global and
// `begin(name)` API, so no extra lib_deps are needed.

#if defined(ESP8266)
#include <ESP8266NetBIOS.h>
#else
#include <NetBIOS.h>
#endif

#include "hw/logger.h"

namespace platform
{
    // Start advertising `hostname` over NetBIOS. Call once, after WiFi is
    // connected (same place as mdnsBegin()). NetBIOS names are case-insensitive
    // and capped at 15 chars; the responder handles truncation. Needs no
    // per-loop polling (it answers from a UDP callback).
    inline void netbiosBegin(const String &hostname)
    {
        if (NBNS.begin(hostname.c_str()))
        {
            LOG("NetBIOS responder started: " + hostname);
        }
        else
        {
            LOG(F("NetBIOS responder failed to start"));
        }
    }
}

#endif
