#include "config.h"
#include "core/configCodec.h"
namespace art
{
Config::Config()
{
    _dirty = false;
    // core::HardwareConfig (config.h's HardwareConfig alias) has no default
    // member initializers - its defaults are board-specific Arduino macros
    // (DEFAULT_PWM_FREQ/LED_PIN/DEFAULT_BUTTON_PIN/DEFAULT_BUTTON_LONG_PRESS),
    // which core/configModel.h can't depend on.
    hardware.pwmFreq = DEFAULT_PWM_FREQ;
    hardware.ledPin = LED_PIN;
    hardware.buttonPin = DEFAULT_BUTTON_PIN;
    hardware.longPressDelay = DEFAULT_BUTTON_LONG_PRESS * 1000;
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
    configToJson(doc);
    if (ESP_FS.exists(CONFIG_FILE) && !ESP_FS.remove(CONFIG_FILE))
        LOG(F("Failed to delete file"));
    File file = ESP_FS.open(CONFIG_FILE, "w");
    if (!file)
    {
        LOG("Failed to create file " + CONFIG_FILE);
        return false;
    }
    // Serialize JSON to file
    LOG(F("Serializing..."));
    if (serializeJson(doc, file) == 0)
    {
        LOG(F("Failed to write JSON to file"));
        return false;
    }
    file.close();
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
    portENTER_CRITICAL(&_pendingMux);
#endif
    size_t written = serializeJson(json, _pendingJson, sizeof(_pendingJson));
    bool ok = written > 0 && written < sizeof(_pendingJson);
    if (ok)
        _hasPending = true;
#ifdef ESP32
    portEXIT_CRITICAL(&_pendingMux);
#endif
    if (!ok)
        LOG(F("Config update too large to stage, dropped"));
    return ok;
}

bool Config::applyPendingUpdate()
{
    if (!_hasPending)
        return false;

    JsonDocument doc;
    DeserializationError error;
#ifdef ESP32
    portENTER_CRITICAL(&_pendingMux);
#endif
    error = deserializeJson(doc, _pendingJson);
    _hasPending = false;
#ifdef ESP32
    portEXIT_CRITICAL(&_pendingMux);
#endif

    if (error)
    {
        LOG(F("Failed to parse staged config update"));
        return false;
    }

    LOG(F("Applying staged config update"));
    if (update(doc.as<JsonVariant>()))
    {
        save();
    }
    else
    {
        LOG("Reloading existing config");
        load();
    }

    String jsonString;
    serialize(jsonString);
    LOG(jsonString);
    return true;
}

void Config::serialize(String &to)
{
    JsonDocument doc;
    configToJson(doc);
    serializeJsonPretty(doc, to);
}

void Config::cleanupWiFi()
{
    wifi.clear();
}

void Config::cleanupDmx()
{
    dmx.clear();
}

void Config::cleanup()
{
    host = DEFAULT_HOST_NAME;
    hardware.pwmFreq = DEFAULT_PWM_FREQ;
    cleanupWiFi();
    cleanupDmx();
}

void Config::configFromJson(JsonVariant doc)
{
    configVersion = doc["configVersion"] | 1;

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

    JsonArray nets = doc["wifi"].as<JsonArray>();
    if (nets.size() > 0)
    {
        LOG("WiFi:");
        cleanupWiFi();
        for (JsonObject net : nets)
        {
            core::WifiNet coreNet = core::wifiNetFromJson(net);
            WiFiNet wifiNet;
            wifiNet.ssid = String(coreNet.ssid.c_str());
            wifiNet.pass = String(coreNet.pass.c_str());
            wifiNet.dhcp = coreNet.dhcp;
            wifiNet.order = coreNet.order;
            wifi.push_back(wifiNet);
            LOG("  " + wifiNet.ssid);
        }
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
            dmx.push_back(deviceConfig);
            LOG("  channel: " + channel["channel"].as<String>());
            LOG("  type: " + channel["type"].as<String>() + " on pin " + channel["pin"].as<String>() + " active: " + channel["level"].as<String>());
        }
    }

    hardware = core::hardwareFromJson(doc["hw"], hardware);
}

void Config::configToJson(JsonDocument &doc)
{
    doc.clear();
    doc["configVersion"] = CONFIG_SCHEMA_VERSION;
    doc["_needReboot"] = _dirty;
    core::hardwareToJson(hardware, doc["hw"].to<JsonObject>());
    doc["host"] = host;
    doc["universe"] = universe;

    JsonArray wifiNets = doc.createNestedArray("wifi");
    for (size_t i = 0; i < wifi.size(); i++)
    {
        const WiFiNet &net = wifi[i];
        core::WifiNet coreNet;
        coreNet.ssid = net.ssid.c_str();
        coreNet.pass = net.pass.c_str();
        coreNet.dhcp = net.dhcp;
        coreNet.order = net.order;
        core::wifiNetToJson(coreNet, wifiNets.createNestedObject());
    }
    JsonArray channels = doc.createNestedArray("dmx");
    for (size_t i = 0; i < dmx.size() && i < MAX_DMX_DEVICES; i++)
    {
        core::dmxChannelToJson(dmx[i], channels.createNestedObject());
    }
}
} // namespace art