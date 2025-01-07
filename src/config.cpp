#include "config.h"
namespace art
{
#include "hw/board.h"
    Config::Config()
    {
        if (ESP_FS.exists(CONFIG_FILE))
        {
            _fileName = CONFIG_FILE;
        }
        else
        {
            LOG(F("Config file does not exist, using default"));
            _fileName = DEFAULT_FILE;
        }
        _dirty = false;
    }

    bool Config::load()
    {
        LOG(F("Loading config from"));
        LOG(_fileName);
        File file = ESP_FS.open(_fileName, "r");
        cleanup();
        StaticJsonDocument<CONFIG_BUFFER_SIZE> doc;
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
        StaticJsonDocument<CONFIG_BUFFER_SIZE> doc;
        configToJson(doc);
        if (!ESP_FS.remove(_fileName))
            LOG(F("Failed to delete file"));
        File file = ESP_FS.open(_fileName, "w");
        if (!file)
        {
            LOG("Failed to create file " + _fileName);
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

    void Config::serialize(String &to)
    {
        StaticJsonDocument<CONFIG_BUFFER_SIZE> doc;
        configToJson(doc);
        serializeJsonPretty(doc, to);
    }

    void Config::cleanupWiFi()
    {
        for (int i = 0; i < wifi.size(); i++)
        {
            delete wifi.get(i);
        }
        wifi.clear();
    }

    void Config::cleanupDmx()
    {
        for (int i = 0; i < dmx.size(); i++)
        {
            delete dmx.get(i);
        }
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
                WiFiNet *wifiNet = new WiFiNet();
                wifiNet->ssid = net["ssid"].as<String>();
                wifiNet->pass = net["pass"].as<String>();
                wifiNet->dhcp = true;
                wifiNet->order = net["order"] | 1;
                wifi.add(wifiNet);
                LOG("  " + wifiNet->ssid);
            }
        }

        JsonArray channels = doc["dmx"].as<JsonArray>();
        if (channels.size() > 0)
        {
            LOG("DMX:");
            cleanupDmx();
            for (JsonObject channel : channels)
            {
                DmxChannel *dmxChannel = new DmxChannel();
                dmxChannel->type = dmxTypeFromString(channel["type"].as<String>());
                dmxChannel->channel = channel["channel"] | 0;
                dmxChannel->level = channel["level"].as<String>().equalsIgnoreCase("high") ? HIGH : LOW;
                dmxChannel->pin = channel["pin"] | LED_BUILTIN;
                dmxChannel->multiplier = channel["multiplier"] | 1;
                dmxChannel->pulse = channel["pulse"] | 10;
                dmxChannel->threshold = channel["threshold"] | 127;
                dmx.add(dmxChannel);
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
        for (int i = 0; i < wifi.size(); i++)
        {
            WiFiNet *net = wifi.get(i);
            wifiNets[i]["ssid"] = net->ssid;
            wifiNets[i]["pass"] = net->pass;
            wifiNets[i]["dhcp"] = net->dhcp;
            wifiNets[i]["order"] = net->order;
        }
        JsonArray channels = doc.createNestedArray("dmx");
        for (int i = 0; i < dmx.size() && i < MAX_DMX_DEVICES; i++)
        {
            DmxChannel *channel = dmx.get(i);
            channels[i]["channel"] = channel->channel;
            channels[i]["type"] = dmxTypeToString(channel->type);
            channels[i]["pin"] = channel->pin;
            channels[i]["level"] = channel->level ? "HIGH" : "LOW";
            channels[i]["multiplier"] = channel->multiplier;
            channels[i]["pulse"] = channel->pulse;
            channels[i]["threshold"] = channel->threshold;
        }
    }
}