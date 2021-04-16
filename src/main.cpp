#include "board.h"

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

#include <ESPAsyncWebServer.h>
#ifndef DISABLE_OTA
#include <AsyncElegantOTA.h>
#endif

//#include <oled.h>
#include <AceButton.h>
#include "version.h"
#include "connect.h"
#include "statusLed.h"
#include "artnetHandler.h"
#include "api.h"
#include "config.h"
#include "device/device.h"
#include "device/dmxServo.h"
#include "device/strobe.h"
#include "device/relay.h"
#include "device/repeater.h"

using namespace ace_button;

#define UDP_PORT 6454 // ArtNet UDP port
#define MAX_DMX_CHANNELS 512

AsyncWebServer server(80);
DNSServer dnsServer;
Connect connect;
art::Config config;
ArtnetHandler artnet;
Device *devices[MAX_DMX_DEVICES];
StatusLed *status;

ButtonConfig buttonConfig;
AceButton button;

#ifdef OLED_SSD1306
#include "oledDisplay.h"
StatusDisplay *statusDisplay;
#endif

void setup()
{
  Serial.begin(115200);
  delay(100);

  if (!ESP_FS.begin())
  {
    LOG(F("Cannot init the filesystem..."));
    return;
  };

  config.load();
  if (config.host.length())
  {
    WiFi.hostname(config.host);
  }
  status = new StatusLed(config.hardware.ledPin, LOW);
  // Buttons
  // Just a single button. Long press (5s) causes a hrd reset of WiFi settings and reboots into a Captive Portal
  buttonConfig.setLongPressDelay(config.hardware.longPressDelay);
  buttonConfig.setFeature(ButtonConfig::kFeatureLongPress);
  button.init(&buttonConfig, config.hardware.buttonPin);
  pinMode(config.hardware.buttonPin, INPUT_PULLUP);
  button.setEventHandler([&](AceButton * /* button */, uint8_t eventType, uint8_t /* buttonState */) {
    switch (eventType)
    {
    case AceButton::kEventLongPressed:
      LOG(F("Hard reset caused by a Button"));
      connect.reset();
      ESP.restart();
      break;
    case AceButton::kEventPressed:
#ifdef OLED_SSD1306
      statusDisplay->message("v." + String(VERSION) + String("\nHold for 5 seconds\nto reset WiFi"));
#endif
      for (int i = 0; i < config.dmx.size() && i < MAX_DMX_DEVICES; i++)
      {
        if (devices[i])
        {
          devices[i]->flip();
        }
      }
      break;
    }
  });
  LOG("Button on pin: " + String(config.hardware.buttonPin) + " long press is: " + String(config.hardware.longPressDelay) + " ms");

#ifdef OLED_SSD1306
  statusDisplay = new StatusDisplay();
  statusDisplay->setConfig(&config);
#endif

  LOG("Init WiFi...");
  connect.init(&server, &dnsServer);
  connect.connect(config.host);

  // TODO: Start mDNS

  // Devices
  for (int i = 0; i < config.dmx.size() && i < MAX_DMX_DEVICES; i++)
  {
    devices[i] = NULL;
    switch (config.dmx[i]->type)
    {
    case art::DmxType::Binary:
      devices[i] = new DmxRelay(1, config.dmx[0]->channel, config.dmx[0]->pin, config.dmx[0]->level, config.dmx[0]->threshold);
      break;
    case art::DmxType::Servo:
      devices[i] = new DmxServo(1, config.dmx[0]->channel, config.dmx[0]->pin);
      break;
    case art::DmxType::Dimmable:
      devices[i] = new Strobe(1, config.dmx[0]->channel, config.dmx[0]->pin, config.dmx[0]->pulse, HIGH, true);
      break;
    case art::DmxType::Repeater:
      devices[i] = new DmxRepeater(1);
      break;
    default:
      LOG(F("Incompatible DMX device type:"));
      LOG(config.dmx[i]->type);
      break;
    }
  }
  artnet.init(1, config.host, config.host, devices, config.dmx.size());

  // Setup WWW
  server.reset();

#ifndef DISABLE_OTA
  AsyncElegantOTA.begin(&server);
#endif

  setupApi(&server, config, &connect, &artnet);
  server.begin(); // Call ONLY AFTER WiFi gets connected !!!!! (or reboot loop)
}

void loop()
{
  artnet.loop();
  button.check();
  for (int i = 0; i < config.dmx.size() && i < MAX_DMX_DEVICES; i++)
    if (devices[i])
      devices[i]->handle();
}