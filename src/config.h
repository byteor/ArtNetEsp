#if !defined(CONFIG_H)
#define CONFIG_H

#if defined(ESP8266)
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <FS.h>
#include <LittleFS.h>
#define ESP_FS LittleFS
#else
#include <WiFi.h>
#include <SPIFFS.h>
#define ESP_FS SPIFFS
#endif

#include <ArduinoJson.h>
#include "hw/logger.h"
#include "hw/board.h"
#include "version.h"

#define MAX_DMX_DEVICES 4 // efficiently a max number of "Art-Net Node Ports"
/*
If more than MAX_DMX_DEVICES are present in the input JSON,
only the first MAX_DMX_DEVICES will be saved and taken into account
*/

namespace art
{
#include <LinkedList.h>

#define CONFIG_BUFFER_SIZE 1024

  typedef struct
  {
    String ssid; // SSID
    String pass;
    bool dhcp;
    uint8_t order; // Order number
  } WiFiNet;

  enum DmxType
  {
    Disabled = 0,
    Binary = 1, // AKA Relay
    Dimmable = 2,
    Servo = 3,
    Repeater = 4, // AKA Art-Net --> DMX gateway
  };

  typedef struct
  {
    uint16_t channel;    // DMX channel
    DmxType type;        // DMX channel features
    uint8_t threshold;   // ON/OFF threshold for a DmxType::Binary switch (relay)
    uint16_t pulse;      // Strobe pulse length
    uint16_t multiplier; // Strobe period multiplier
    uint8_t pin;         // Pin (Arduino numbering)
    uint8_t level;       // Active level: 0=LOW, 1=HIGH
  } DmxChannel;

  typedef struct
  {
    // PWM frequency
    uint16_t pwmFreq = DEFAULT_PWM_FREQ; // ESP8266 default is 1000 which may cause MOSFER overheat
    uint8_t ledPin = LED_PIN;
    uint8_t buttonPin = DEFAULT_BUTTON_PIN;
    uint16_t longPressDelay = DEFAULT_BUTTON_LONG_PRESS * 1000;
  } HardwareConfig;

  class Config
  {
    String _fileName;
    bool _dirty; // A flag that indicates that the module needs a reboot due to a changed config

  protected:
    const String DMX_DISABLED = String("DISABLED");
    const String DMX_BINARY = String("BINARY");
    const String DMX_DIMMABLE = String("DIMMER");
    const String DMX_SERVO = String("SERVO");
    const String DMX_REPEATER = String("REPEATER");

    const String DEFAULT_FILE = String("/config/default.json");
    const String CONFIG_FILE = String("/config/config.json");

    const String DEFAULT_HOST_NAME = String("Art-" + CHIP_ID);

    DmxType dmxTypeFromString(String type)
    {
      if (DMX_BINARY.equals(type))
        return DmxType::Binary;
      if (DMX_DIMMABLE.equals(type))
        return DmxType::Dimmable;
      if (DMX_SERVO.equals(type))
        return DmxType::Servo;
      if (DMX_REPEATER.equals(type))
        return DmxType::Repeater;
      return DmxType::Disabled;
    }

    void configFromJson(JsonVariant doc);
    void configToJson(JsonDocument &doc);
    void cleanup();
    void cleanupWiFi();
    void cleanupDmx();

  public:
    String dmxTypeToString(DmxType type)
    {
      if (type == DmxType::Binary)
        return DMX_BINARY;
      if (type == DmxType::Dimmable)
        return DMX_DIMMABLE;
      if (type == DmxType::Servo)
        return DMX_SERVO;
      if (type == DmxType::Repeater)
        return DMX_REPEATER;
      return DMX_DISABLED;
    }

    // DMX
    art::LinkedList<DmxChannel *> dmx;
    // WiFi
    art::LinkedList<WiFiNet *> wifi;
    String host;
    HardwareConfig hardware;

    Config();
    bool load();
    bool update(JsonVariant json);
    bool save();
    void serialize(String &to);
  };

}

#endif