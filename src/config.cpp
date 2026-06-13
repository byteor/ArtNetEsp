#include "config.h"
namespace art
{
Config::Config()
{
    _dirty = false;
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
            WiFiNet wifiNet;
            wifiNet.ssid = net["ssid"].as<String>();
            wifiNet.pass = net["pass"].as<String>();
            wifiNet.dhcp = true;
            wifiNet.order = net["order"] | 1;
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
        for (JsonObject channel : channels)
        {
            if (dmx.size() >= MAX_DMX_DEVICES)
                break;
            DeviceConfig deviceConfig;
            deviceConfig.type = dmxTypeFromString(channel["type"].as<String>());
            deviceConfig.channel = channel["channel"] | 0;
            deviceConfig.level = channel["level"].as<String>().equalsIgnoreCase("high") ? HIGH : LOW;
            deviceConfig.pin = channel["pin"] | LED_BUILTIN;
            deviceConfig.multiplier = channel["multiplier"] | 1;
            deviceConfig.pulse = channel["pulse"] | 10;
            deviceConfig.threshold = channel["threshold"] | 127;
            deviceConfig.blackout = channel["blackout"] | true;
            dmx.push_back(deviceConfig);
            LOG("  channel: " + channel["channel"].as<String>());
            LOG("  type: " + channel["type"].as<String>() + " on pin " + channel["pin"].as<String>() + " active: " + channel["level"].as<String>());
        }
    }

    if (!doc["hw"]["freq"].isNull())
    {
        hardware.pwmFreq = doc["hw"]["freq"] | DEFAULT_PWM_FREQ;
        LOG("PWM: " + doc["hw"]["freq"].as<String>() + " Hz");
    }
    if (!doc["hw"]["ledPin"].isNull())
    {
        hardware.ledPin = doc["hw"]["ledPin"] | LED_PIN;
        LOG("Led i/o: " + String(hardware.ledPin));
    }
    if (!doc["hw"]["buttonPin"].isNull())
    {
        hardware.buttonPin = doc["hw"]["buttonPin"] | DEFAULT_BUTTON_PIN;
        LOG("Button i/o: " + String(hardware.buttonPin));
    }
    if (!doc["hw"]["longPressDelay"].isNull())
    {
        hardware.longPressDelay = doc["hw"]["longPressDelay"] | DEFAULT_BUTTON_LONG_PRESS * 1000;
        LOG("Long press: " + String(hardware.longPressDelay) + " ms");
    }
}

void Config::configToJson(JsonDocument &doc)
{
    doc.clear();
    doc["configVersion"] = CONFIG_SCHEMA_VERSION;
    doc["_needReboot"] = _dirty;
    doc["hw"]["freq"] = hardware.pwmFreq;
    doc["hw"]["ledPin"] = hardware.ledPin;
    doc["hw"]["buttonPin"] = hardware.buttonPin;
    doc["hw"]["longPressDelay"] = hardware.longPressDelay;
    doc["info"]["id"] = CHIP_ID;
    doc["info"]["chip"] = CHIP_ARC;
    doc["info"]["version"] = VERSION;
    doc["info"]["built"] = BUILD_TIMESTAMP;
    doc["info"]["max_dmx_devices"] = MAX_DMX_DEVICES;
    doc["info"]["ssid"] = WiFi.SSID();
    doc["info"]["rssi"] = WiFi.RSSI();
    doc["host"] = host;
    doc["universe"] = universe;

    JsonArray wifiNets = doc.createNestedArray("wifi");
    for (size_t i = 0; i < wifi.size(); i++)
    {
        const WiFiNet &net = wifi[i];
        wifiNets[i]["ssid"] = net.ssid;
        wifiNets[i]["pass"] = net.pass;
        wifiNets[i]["dhcp"] = net.dhcp;
        wifiNets[i]["order"] = net.order;
    }
    JsonArray channels = doc.createNestedArray("dmx");
    for (size_t i = 0; i < dmx.size() && i < MAX_DMX_DEVICES; i++)
    {
        const DeviceConfig &channel = dmx[i];
        channels[i]["channel"] = channel.channel;
        channels[i]["type"] = dmxTypeToString(channel.type);
        channels[i]["pin"] = channel.pin;
        channels[i]["level"] = channel.level ? "HIGH" : "LOW";
        channels[i]["multiplier"] = channel.multiplier;
        channels[i]["pulse"] = channel.pulse;
        channels[i]["threshold"] = channel.threshold;
        channels[i]["blackout"] = channel.blackout;
    }
}
} // namespace art