#pragma once

// Platform-free (no Arduino.h) - safe to include from native tests and from
// any translation unit, including under FEATURE_DMX_PORT=0 (SONOFF boards).

#include <cstdint>
#include <cstring>

namespace core
{

enum class DmxType : uint8_t
{
    Disabled = 0,
    Relay = 1,
    Dimmer = 2,
    Servo = 3,
    Repeater = 4, // AKA Art-Net --> DMX gateway
};

// Full DMX universe size (channels 1-512) and the on-the-wire packet size
// (start code + 512 channel slots, B5). dmx/dmx.h defines its own
// DMX_CHANNELS/DMX_BUFFER_SIZE macros for the same values - this is the
// canonical, platform-free home for code outside dmx/.
constexpr uint16_t DMX_CHANNELS = 512;
constexpr uint16_t DMX_PACKET_SIZE = DMX_CHANNELS + 1;

// Wire string for each type, as used in config JSON. Relay's wire string is
// the legacy "BINARY" (R5 compat contract) - never "RELAY".
inline const char *toWireString(DmxType type)
{
    switch (type)
    {
    case DmxType::Relay:
        return "BINARY";
    case DmxType::Dimmer:
        return "DIMMER";
    case DmxType::Servo:
        return "SERVO";
    case DmxType::Repeater:
        return "REPEATER";
    case DmxType::Disabled:
    default:
        return "DISABLED";
    }
}

// Accepts both the legacy "BINARY" and the canonical "RELAY" alias for
// DmxType::Relay; anything unrecognized maps to Disabled.
inline DmxType fromWireString(const char *type)
{
    if (strcmp(type, "BINARY") == 0 || strcmp(type, "RELAY") == 0)
        return DmxType::Relay;
    if (strcmp(type, "DIMMER") == 0)
        return DmxType::Dimmer;
    if (strcmp(type, "SERVO") == 0)
        return DmxType::Servo;
    if (strcmp(type, "REPEATER") == 0)
        return DmxType::Repeater;
    return DmxType::Disabled;
}

} // namespace core
