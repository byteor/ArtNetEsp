#pragma once

// Platform-free (no Arduino.h) ArduinoJson <-> configModel conversions.
// config.cpp calls these for the dmx[]/hw/wifi[] portions of
// configFromJson/configToJson; host/universe/info/_needReboot stay in
// config.cpp since they're either Arduino-typed (String) or not part of
// the per-entry model.
//
// Header-only (inline), matching the core/dmxTypes.h / core/dimmerLogic.h
// pattern - env:native's build_src_filter excludes src/, so a separate .cpp
// here wouldn't be compiled into the native test build.

#include <cstring>

#include <ArduinoJson.h>

#include "core/configModel.h"

namespace core
{

// Reads one dmx[] entry. Any field missing/null in obj falls back to the
// corresponding field in defaults (obj["type"] falls back to
// fromWireString("") = DmxType::Disabled regardless of defaults.type).
inline DeviceConfig dmxChannelFromJson(JsonVariantConst obj, const DeviceConfig &defaults)
{
    DeviceConfig cfg;
    const char *typeStr = obj["type"] | "";
    cfg.type = fromWireString(typeStr);
    cfg.channel = obj["channel"] | defaults.channel;
    const char *levelStr = obj["level"] | "";
    cfg.level = (strcasecmp(levelStr, "high") == 0) ? 1 : 0;
    cfg.pin = obj["pin"] | defaults.pin;
    cfg.multiplier = obj["multiplier"] | defaults.multiplier;
    cfg.pulse = obj["pulse"] | defaults.pulse;
    cfg.threshold = obj["threshold"] | defaults.threshold;
    cfg.blackout = obj["blackout"] | defaults.blackout;
    return cfg;
}

inline void dmxChannelToJson(const DeviceConfig &cfg, JsonObject obj)
{
    obj["channel"] = cfg.channel;
    obj["type"] = toWireString(cfg.type);
    obj["pin"] = cfg.pin;
    obj["level"] = cfg.level ? "HIGH" : "LOW";
    obj["multiplier"] = cfg.multiplier;
    obj["pulse"] = cfg.pulse;
    obj["threshold"] = cfg.threshold;
    obj["blackout"] = cfg.blackout;
}

// Reads the "hw" object. Any field missing/null in obj is left as the
// corresponding field in defaults.
inline HardwareConfig hardwareFromJson(JsonVariantConst obj, const HardwareConfig &defaults)
{
    HardwareConfig hw = defaults;
    if (!obj["freq"].isNull())
        hw.pwmFreq = obj["freq"] | defaults.pwmFreq;
    if (!obj["ledPin"].isNull())
        hw.ledPin = obj["ledPin"] | defaults.ledPin;
    if (!obj["buttonPin"].isNull())
        hw.buttonPin = obj["buttonPin"] | defaults.buttonPin;
    if (!obj["longPressDelay"].isNull())
        hw.longPressDelay = obj["longPressDelay"] | defaults.longPressDelay;
    if (!obj["wifiPowerSave"].isNull())
        hw.wifiPowerSave = obj["wifiPowerSave"] | defaults.wifiPowerSave;
    if (!obj["authEnabled"].isNull())
        hw.authEnabled = obj["authEnabled"] | defaults.authEnabled;
    if (!obj["authUser"].isNull())
        hw.authUser = std::string(obj["authUser"] | "");
    if (!obj["authPass"].isNull())
        hw.authPass = std::string(obj["authPass"] | "");
    return hw;
}

inline void hardwareToJson(const HardwareConfig &hw, JsonObject obj)
{
    obj["freq"] = hw.pwmFreq;
    obj["ledPin"] = hw.ledPin;
    obj["buttonPin"] = hw.buttonPin;
    obj["longPressDelay"] = hw.longPressDelay;
    obj["wifiPowerSave"] = hw.wifiPowerSave;
    obj["authEnabled"] = hw.authEnabled;
    obj["authUser"] = hw.authUser.c_str();
    obj["authPass"] = hw.authPass.c_str();
}

// dhcp is always true on read (matches the pre-extraction configFromJson,
// which never reads a "dhcp" key from JSON).
inline WifiNet wifiNetFromJson(JsonVariantConst obj)
{
    WifiNet net;
    net.ssid = std::string(obj["ssid"] | "");
    net.pass = std::string(obj["pass"] | "");
    net.dhcp = true;
    net.order = obj["order"] | 1;
    return net;
}

inline void wifiNetToJson(const WifiNet &net, JsonObject obj)
{
    obj["ssid"] = net.ssid.c_str();
    obj["pass"] = net.pass.c_str();
    obj["dhcp"] = net.dhcp;
    obj["order"] = net.order;
}

} // namespace core
