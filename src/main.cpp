#include "hw/board.h"

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
#include <ElegantOTA.h>
#endif

#ifdef ESP32
#include <analogWrite.h>
//  https://github.com/espressif/arduino-esp32/issues/863#issuecomment-347179737
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

#include <AceButton.h>
#include "version.h"
#include "connect.h"
#include "hw/statusLed.h"
#include "artnetHandler.h"
#include "api.h"
#include "config.h"
#include "device/device.h"
#include "device/relay.h"
#ifndef SONOFF_BASIC
#include "device/strobe.h"
#include "device/repeater.h"
#include "device/dmxServo.h"
#endif

using namespace ace_button;

AsyncWebServer server(80);
DNSServer dnsServer;
Connect connect;
art::Config config;
ArtnetHandler artnet;
Device *dmx_devices[MAX_DMX_DEVICES];
StatusLed *status;

ButtonConfig buttonConfig;
AceButton button;

#ifdef OLED_SSD1306
#include "hw/oledDisplay.h"
StatusDisplay *statusDisplay;
#endif

void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
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
    LOG(F("Button pressed"));
    for (int i = 0; i < config.dmx.size() && i < MAX_DMX_DEVICES; i++)
    {
      if (dmx_devices[i])
      {
        dmx_devices[i]->flip();
      }
    }
    if (config.dmx.size() > 0)
    {
      if (dmx_devices[0]->isEnabled())
      {
        status->set(StatusLed::Status::ON);
      }
      else
      {
        status->set(StatusLed::Status::OFF);
      }
    }
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);

#if defined(ESP8266)
  if (!ESP_FS.begin())
#else
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
  if (!ESP_FS.begin(true))
#endif
  {
    LOG(F("Cannot init the filesystem..."));
    return;
  };
  delay(100);

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
  button.setEventHandler(handleEvent);

  LOG("Button on pin: " + String(config.hardware.buttonPin) + " long press is: " + String(config.hardware.longPressDelay) + " ms");

#ifdef OLED_SSD1306
  statusDisplay = new StatusDisplay();
  statusDisplay->setConfig(&config);
#endif

  // Devices
#ifdef ESP32
  // TODO: investigate - analogWriteFrequency doesn't compile despite it is defined in analogWrite library
  // analogWriteFrequency((double)config.hardware.pwmFreq);
#else
  analogWriteFreq(config.hardware.pwmFreq);
  analogWriteRange(255);
#endif

  for (int i = 0; i < config.dmx.size() && i < MAX_DMX_DEVICES; i++)
  {
    dmx_devices[i] = NULL;
    switch (config.dmx[i]->type)
    {
    case art::DmxType::Binary:
      dmx_devices[i] = new DmxRelay(1, config.dmx[i]->channel, config.dmx[i]->pin, config.dmx[i]->level, config.dmx[i]->threshold);
      break;
#ifndef SONOFF_BASIC
    case art::DmxType::Servo:
      dmx_devices[i] = new DmxServo(1, config.dmx[i]->channel, config.dmx[i]->pin);
      break;
    case art::DmxType::Dimmable:
      dmx_devices[i] = new Strobe(1, config.dmx[i]->channel, config.dmx[i]->pin, config.dmx[i]->pulse, config.dmx[i]->multiplier, config.dmx[i]->level);
      break;
    case art::DmxType::Repeater:
      dmx_devices[i] = new DmxRepeater(1);
      break;
#endif
    default:
      LOG(F("Incompatible DMX device type:"));
      LOG(config.dmx[i]->type);
      break;
    }
  }

  LOG("Init WiFi...");
  connect.init(&server, &dnsServer);
  connect.connect(config.host);

  // TODO: Start mDNS

  // ArtNet
  String longName = config.host;
  if (config.dmx.size() > 0)
  {
    for (int i = 0; i < config.dmx.size(); i++)
    {
      longName += " " + config.dmxTypeToString(config.dmx[i]->type) + " #" + String(config.dmx[i]->channel);
    }
  }
  LOG("ArtNet Node: " + config.host + " : " + longName);
  artnet.init(config.universe, config.host, longName, dmx_devices, config.dmx.size());

  // Setup WWW
  server.reset();

#ifndef DISABLE_OTA
  ElegantOTA.begin(&server);
#endif

  setupApi(&server, config, &connect);
  server.begin(); // Call ONLY AFTER WiFi gets connected !!!!! (or reboot loop)
}

void loop()
{
  button.check();
  artnet.loop();
  for (int i = 0; i < config.dmx.size() && i < MAX_DMX_DEVICES; i++)
    if (dmx_devices[i])
      dmx_devices[i]->handle();

#ifndef DISABLE_OTA
  ElegantOTA.loop();
#endif
  connect.loop();
}