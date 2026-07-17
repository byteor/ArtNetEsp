#pragma once

// Platform-free (no Arduino.h) - safe to include from native tests and from
// any translation unit. Mirrors the wire shapes art::DeviceConfig/HardwareConfig
// (config.h) serialize to/from - see core/configCodec.h for the ArduinoJson
// conversions.

#include <cstdint>
#include <string>

#include "core/dmxTypes.h"

namespace core
{

struct DeviceConfig
{
    uint16_t channel;    // DMX channel
    DmxType type;        // DMX channel features
    uint8_t threshold;   // ON/OFF threshold for a DmxType::Relay switch
    uint16_t pulse;      // Strobe pulse length
    uint16_t multiplier; // Strobe period multiplier
    uint8_t pin;         // Pin (Arduino numbering)
    uint8_t level;       // Active level: 0=LOW, 1=HIGH
    bool blackout = true; // B20: blackout this device on DMX silence
};

struct HardwareConfig
{
    uint16_t pwmFreq;
    uint8_t ledPin;
    uint8_t buttonPin;
    uint16_t longPressDelay;
    // WiFi modem-sleep policy. false (default) keeps the radio always on for
    // the lowest Art-Net receive latency/jitter and tightest multi-device sync
    // on WiFi; true re-enables modem sleep to save power. Old configs without
    // "hw.wifiPowerSave" default to false (radio always on).
    bool wifiPowerSave = false;
    // Optional HTTP basic-auth for mutating REST routes and /update (OTA).
    // Off by default - old configs without "hw.authEnabled" are unaffected.
    bool authEnabled = false;
    std::string authUser;
    std::string authPass;
};

// Conservative GPIO sanity check for user-supplied pin numbers (hw.ledPin,
// hw.buttonPin, dmx[].pin). Deliberately NOT a full per-variant pin map -
// just enough to reject pins that would corrupt flash (ESP8266 GPIO6-11)
// or don't exist on any supported board.
inline bool isValidGpio(uint8_t pin, bool isEsp32)
{
    if (isEsp32)
        return pin <= 48; // classic/S2/S3 families top out at GPIO48 (S3)
    return pin <= 16 && !(pin >= 6 && pin <= 11); // ESP8266: GPIO0-5, 12-16
}

} // namespace core
