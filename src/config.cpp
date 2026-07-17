#include "config.h"
#include "core/configCodec.h"
namespace art
{
// cppcheck-suppress uninitMemberVar  // _pendingJson (2-4KB, config.h) is only ever read after stageUpdate() fully populates it (gated by _hasPending); zero-initializing it here would just cost boot-time cycles for no safety benefit
Config::Config()
{
    _dirty = false;
    // core::HardwareConfig's (config.h's HardwareConfig alias) in-class
    // member initializers for these four fields are just placeholder zeros
    // (core/configModel.h can't depend on Arduino board macros) - their real
    // defaults are board-specific Arduino macros
    // (DEFAULT_PWM_FREQ/LED_PIN/DEFAULT_BUTTON_PIN/DEFAULT_BUTTON_LONG_PRESS),
    // applied here unconditionally.
    hardware.pwmFreq = DEFAULT_PWM_FREQ;
    hardware.ledPin = LED_PIN;
    hardware.buttonPin = DEFAULT_BUTTON_PIN;
    hardware.longPressDelay = DEFAULT_BUTTON_LONG_PRESS * 1000;
#ifdef ESP32
    _pendingMutex = xSemaphoreCreateMutex();
#endif
}

bool Config::load()
{
    String fileName = CONFIG_FILE;
    if (!ESP_FS.exists(CONFIG_FILE))
    {
        LOG(F("Config file does not exist, using default"));
        fileName = DEFAULT_FILE;
    }
    LOG(F("Loading config from"));
    LOG(fileName);
    File file = ESP_FS.open(fileName, "r");
    cleanup();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        LOG(F("Failed to read file"));
        return false;
    }
    else
    {
        configFromJson(doc.as<JsonVariant>());
        LOG(F("Loaded config"));
    }
    return true;
};

bool Config::save()
{
    LOG(F("Saving config..."));
    JsonDocument doc;
    configToJson(doc, /*includeSecrets=*/true);
    // Write to a temp file, then rename over the old config (B4). LittleFS
    // rename is atomic, so a power cut mid-save can no longer leave the
    // device with no config at all (the old delete-then-rewrite could).
    const String tmpFile = CONFIG_FILE + ".new";
    File file = ESP_FS.open(tmpFile, "w");
    if (!file)
    {
        LOG("Failed to create file " + tmpFile);
        return false;
    }
    LOG(F("Serializing..."));
    if (serializeJson(doc, file) == 0)
    {
        LOG(F("Failed to write JSON to file"));
        file.close();
        ESP_FS.remove(tmpFile);
        return false;
    }
    file.close();
    if (!ESP_FS.rename(tmpFile, CONFIG_FILE))
    {
        // Some FS wrappers won't rename over an existing file - clear the
        // destination and retry.
        ESP_FS.remove(CONFIG_FILE);
        if (!ESP_FS.rename(tmpFile, CONFIG_FILE))
        {
            LOG(F("Failed to move new config into place"));
            return false;
        }
    }
    LOG(F("Config saved"));
    return true;
};

bool Config::update(JsonVariant json)
{
    configFromJson(json);
    LOG(F("Updated config"));
    _dirty = true;
    return true;
}

bool Config::stageUpdate(JsonVariant json)
{
#ifdef ESP32
    xSemaphoreTake(_pendingMutex, portMAX_DELAY);
#endif
    size_t written = serializeJson(json, _pendingJson, sizeof(_pendingJson));
    bool ok = written > 0 && written < sizeof(_pendingJson);
    if (ok)
        _hasPending.store(true, std::memory_order_release);
#ifdef ESP32
    xSemaphoreGive(_pendingMutex);
#endif
    if (!ok)
        LOG(F("Config update too large to stage, dropped"));
    return ok;
}

bool Config::applyPendingUpdate()
{
    if (!_hasPending.load(std::memory_order_acquire))
        return false;

    // Parse under the mutex, but force ArduinoJson's copying mode by passing
    // const char* - with a mutable char* it would keep pointers INTO
    // _pendingJson, which a concurrent stageUpdate could overwrite once the
    // mutex is released. With the copy, doc is self-contained and
    // update()/save() safely run outside the lock.
    JsonDocument doc;
    DeserializationError error;
#ifdef ESP32
    xSemaphoreTake(_pendingMutex, portMAX_DELAY);
#endif
    error = deserializeJson(doc, static_cast<const char *>(_pendingJson));
    _hasPending.store(false, std::memory_order_release);
#ifdef ESP32
    xSemaphoreGive(_pendingMutex);
#endif

    if (error)
    {
        LOG(F("Failed to parse staged config update"));
        return false;
    }

    LOG(F("Applying staged config update"));
    // cppcheck-suppress knownConditionTrueFalse  // update() always returns true today (configFromJson has no failure path); the reload-on-failure branch below is deliberate defensive dead code, not a bug - making update() fallible is a real design change out of scope here
    if (update(doc.as<JsonVariant>()))
    {
        save();
    }
    else
    {
        LOG("Reloading existing config");
        load();
    }
    return true;
}

void Config::cleanupDmx()
{
    dmx.clear();
}

void Config::cleanup()
{
    host = DEFAULT_HOST_NAME;
    hardware.pwmFreq = DEFAULT_PWM_FREQ;
    cleanupDmx();
}

void Config::configFromJson(JsonVariant doc)
{
    configVersion = doc["configVersion"] | 1;
#ifdef ESP32
    constexpr bool kIsEsp32 = true;
#else
    constexpr bool kIsEsp32 = false;
#endif

    if (!doc["host"].isNull())
    {
        host = doc["host"].as<String>();
        LOG("Host: " + host);
    }
    if (!doc["universe"].isNull())
    {
        universe = doc["universe"].as<unsigned int>();
        LOG("Universe: " + String(universe));
    }

    JsonArray channels = doc["dmx"].as<JsonArray>();
    if (channels.size() > 0)
    {
        LOG("DMX:");
        cleanupDmx();
        if (channels.size() > MAX_DMX_DEVICES)
            LOG("  WARNING: " + String(channels.size()) + " dmx entries in config, only the first " + String(MAX_DMX_DEVICES) + " will be used");
        DeviceConfig defaults;
        defaults.channel = 0;
        defaults.type = DmxType::Disabled;
        defaults.threshold = 127;
        defaults.pulse = 10;
        defaults.multiplier = 1;
        defaults.pin = LED_BUILTIN;
        defaults.level = LOW;
        defaults.blackout = true;
        for (JsonObject channel : channels)
        {
            if (dmx.size() >= MAX_DMX_DEVICES)
                break;
            DeviceConfig deviceConfig = core::dmxChannelFromJson(channel, defaults);
            // Repeater uses the fixed DMX UART pins, Disabled uses nothing -
            // only pin-driving types need a valid GPIO.
            if (deviceConfig.type != DmxType::Repeater && deviceConfig.type != DmxType::Disabled &&
                !core::isValidGpio(deviceConfig.pin, kIsEsp32))
            {
                LOG("Rejecting device with invalid pin " + String(deviceConfig.pin));
                deviceConfig.type = DmxType::Disabled;
            }
            dmx.push_back(deviceConfig);
            LOG("  channel: " + channel["channel"].as<String>());
            LOG("  type: " + channel["type"].as<String>() + " on pin " + channel["pin"].as<String>() + " active: " + channel["level"].as<String>());
        }
    }

    HardwareConfig parsedHw = core::hardwareFromJson(doc["hw"], hardware);
    if (!core::isValidGpio(parsedHw.ledPin, kIsEsp32))
    {
        LOG("Rejecting invalid ledPin " + String(parsedHw.ledPin));
        parsedHw.ledPin = hardware.ledPin;
    }
    if (!core::isValidGpio(parsedHw.buttonPin, kIsEsp32))
    {
        LOG("Rejecting invalid buttonPin " + String(parsedHw.buttonPin));
        parsedHw.buttonPin = hardware.buttonPin;
    }
    hardware = parsedHw;
}

void Config::configToJson(JsonDocument &doc, bool includeSecrets)
{
    doc.clear();
    doc["configVersion"] = CONFIG_SCHEMA_VERSION;
    doc["_needReboot"] = _dirty;
    core::hardwareToJson(hardware, doc["hw"].to<JsonObject>(), includeSecrets);
    doc["host"] = host;
    doc["universe"] = universe;

    JsonArray channels = doc["dmx"].to<JsonArray>();
    for (size_t i = 0; i < dmx.size() && i < MAX_DMX_DEVICES; i++)
    {
        core::dmxChannelToJson(dmx[i], channels.add<JsonObject>());
    }
}
} // namespace art