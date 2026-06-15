#pragma once

// Platform-free (no Arduino.h) - safe to include from native tests and from
// any translation unit. Mirrors the wire shapes art::DeviceConfig/HardwareConfig/
// WiFiNet (config.h) serialize to/from - see core/configCodec.h for the
// ArduinoJson conversions.

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

// Platform-free counterpart to art::WiFiNet (config.h), which uses Arduino
// String - configCodec/config.cpp convert between the two at the boundary.
struct WifiNet
{
    std::string ssid;
    std::string pass;
    bool dhcp;
    uint8_t order;
};

} // namespace core
