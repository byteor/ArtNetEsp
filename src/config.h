#if !defined(CONFIG_H)
#define CONFIG_H

#include "platform/filesystem.h"

#include <ArduinoJson.h>
#include <vector>
#include "hw/logger.h"
#include "boards/board.h"
#include "core/dmxTypes.h"
#include "Version.h"

// efficiently a max number of "Art-Net Node Ports" (DMX devices) that can be configured
#ifdef ESP32
#define MAX_DMX_DEVICES 8
#else
#define MAX_DMX_DEVICES 4
#endif
/*
If more than MAX_DMX_DEVICES are present in the input JSON,
only the first MAX_DMX_DEVICES will be saved and taken into account
*/

namespace art
{
#define CONFIG_BUFFER_SIZE 1024

// Schema version written by configToJson(); bumped whenever a future config
// migration needs to distinguish old/new on-disk JSON shapes. configFromJson
// reads "configVersion" (defaulting to 1 for configs that predate this field)
// but doesn't act on it yet - this just establishes the migration hook.
#define CONFIG_SCHEMA_VERSION 1

typedef struct
{
    String ssid; // SSID
    String pass;
    bool dhcp;
    uint8_t order; // Order number
} WiFiNet;

// core::DmxType is the canonical enum (src/core/dmxTypes.h, platform-free);
// this alias preserves the art::DmxType spelling used throughout this codebase.
using DmxType = core::DmxType;

typedef struct
{
    uint16_t channel;    // DMX channel
    DmxType type;        // DMX channel features
    uint8_t threshold;   // ON/OFF threshold for a DmxType::Relay switch
    uint16_t pulse;      // Strobe pulse length
    uint16_t multiplier; // Strobe period multiplier
    uint8_t pin;         // Pin (Arduino numbering)
    uint8_t level;       // Active level: 0=LOW, 1=HIGH
    bool blackout = true; // B20: blackout this device on DMX silence (Device::setBlackout)
} DeviceConfig;

typedef struct
{
    // PWM frequency
    uint16_t pwmFreq = DEFAULT_PWM_FREQ; // ESP8266 default is 1000 which may cause some MOSFET overheat
    uint8_t ledPin = LED_PIN;
    uint8_t buttonPin = DEFAULT_BUTTON_PIN;
    uint16_t longPressDelay = DEFAULT_BUTTON_LONG_PRESS * 1000;
} HardwareConfig;

class Config
{
    bool _dirty; // A flag that indicates that the module needs a reboot due to a changed config

    // B11: POST /config (on ESP32, the async_tcp task) stages the raw JSON
    // here instead of touching config.dmx/config.wifi directly - those
    // vectors are read every loop() iteration (device handle(), the
    // button handler, StatusDisplay). applyPendingUpdate() is called from
    // loop(), so the actual update()/cleanupDmx()/cleanupWiFi()/save() runs
    // single-threaded, on the same task as the readers.
    char _pendingJson[CONFIG_BUFFER_SIZE];
    volatile bool _hasPending = false;
#ifdef ESP32
    portMUX_TYPE _pendingMux = portMUX_INITIALIZER_UNLOCKED;
#endif

protected:
    const String DEFAULT_FILE = String("/config/default.json");
    const String CONFIG_FILE = String("/config/config.json");

    const String DEFAULT_HOST_NAME = String("Art-" + CHIP_ID);

    // Thin adapters over core::fromWireString/toWireString (src/core/dmxTypes.h)
    // - kept here so existing call sites (Config::dmxTypeFromString/ToString)
    // don't need to change.
    DmxType dmxTypeFromString(String type)
    {
        return core::fromWireString(type.c_str());
    }

    void configFromJson(JsonVariant doc);
    void configToJson(JsonDocument &doc);
    void cleanup();
    void cleanupWiFi();
    void cleanupDmx();

public:
    String dmxTypeToString(DmxType type)
    {
        return String(core::toWireString(type));
    }

    // DMX
    std::vector<DeviceConfig> dmx;
    // WiFi
    std::vector<WiFiNet> wifi;
    String host;
    unsigned int universe = 0;
    HardwareConfig hardware;
    // Schema version of the config this instance was loaded from (defaults
    // to 1 for configs written before this field existed). configToJson()
    // always writes CONFIG_SCHEMA_VERSION, not this value - migrations (none
    // yet) would compare this against CONFIG_SCHEMA_VERSION in configFromJson.
    uint8_t configVersion = CONFIG_SCHEMA_VERSION;

    Config();
    bool load();
    bool update(JsonVariant json);
    bool save();
    void serialize(String &to);

    // B11: stage a POST /config payload for application from loop().
    // Returns false (and drops the update) if it doesn't fit CONFIG_BUFFER_SIZE.
    bool stageUpdate(JsonVariant json);
    // B11: called from loop(); applies + saves a staged update, if any.
    // Returns false if there was nothing pending.
    bool applyPendingUpdate();
};

} // namespace art

#endif