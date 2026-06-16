#if !defined(CONFIG_H)
#define CONFIG_H

#include "platform/filesystem.h"

#include <ArduinoJson.h>
#include <vector>
#include "hw/logger.h"
#include "boards/board.h"
#include "core/dmxTypes.h"
#include "core/configModel.h"
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
// Staging buffer for a raw POST /config body (see Config::_pendingJson /
// stageUpdate). Sized per-platform to hold a full config including the max
// device count (ESP32 8, ESP8266 4) - the web UI sends section-scoped updates,
// but a full dmx[] save must still fit. Stays well under
// AsyncCallbackJsonWebHandler's 16 KB content-length limit.
#ifdef ESP32
#define CONFIG_BUFFER_SIZE 4096
#else
#define CONFIG_BUFFER_SIZE 2048
#endif

// Schema version written by configToJson(); bumped whenever a future config
// migration needs to distinguish old/new on-disk JSON shapes. configFromJson
// reads "configVersion" (defaulting to 1 for configs that predate this field)
// but doesn't act on it yet - this just establishes the migration hook.
#define CONFIG_SCHEMA_VERSION 1

// core::DmxType is the canonical enum (src/core/dmxTypes.h, platform-free);
// this alias preserves the art::DmxType spelling used throughout this codebase.
using DmxType = core::DmxType;

// DeviceConfig/HardwareConfig are the platform-free PODs from
// core/configModel.h - these aliases preserve the art:: spelling used
// throughout this codebase.
using DeviceConfig = core::DeviceConfig;
using HardwareConfig = core::HardwareConfig;

class Config
{
    bool _dirty; // A flag that indicates that the module needs a reboot due to a changed config

    // B11: POST /config (on ESP32, the async_tcp task) stages the raw JSON
    // here instead of touching config.dmx directly - that vector is read
    // every loop() iteration (device handle(), the button handler,
    // StatusDisplay). applyPendingUpdate() is called from loop(), so the
    // actual update()/cleanupDmx()/save() runs single-threaded, on the same
    // task as the readers.
    char _pendingJson[CONFIG_BUFFER_SIZE];
    volatile bool _hasPending = false;
#ifdef ESP32
    portMUX_TYPE _pendingMux = portMUX_INITIALIZER_UNLOCKED;
#endif

protected:
    const String DEFAULT_FILE = String("/config/default.json");
    const String CONFIG_FILE = String("/config/config.json");

    const String DEFAULT_HOST_NAME = String("Art-" + CHIP_ID);

    void configFromJson(JsonVariant doc);
    void configToJson(JsonDocument &doc);
    void cleanup();
    void cleanupDmx();

public:
    String dmxTypeToString(DmxType type)
    {
        return String(core::toWireString(type));
    }

    // Serializes the persisted config (configVersion/_needReboot/hw/host/
    // universe/dmx) to doc. Does NOT include "info" - that's runtime
    // WiFi/build/chip identity, not part of the persisted config; webApi's
    // GET /config and GET /status add it separately (§1.2.9 layering fix).
    void toJson(JsonDocument &doc)
    {
        configToJson(doc);
    }

    // Whether a config change has been applied that needs a reboot to take
    // effect (the `_needReboot` flag exposed by GET /config and GET /status).
    // Reset to false on boot (constructor) - not persisted - so it clears after
    // a reboot, letting the web UI drop its "reboot required" banner.
    bool needsReboot() const { return _dirty; }

    // DMX
    std::vector<DeviceConfig> dmx;
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