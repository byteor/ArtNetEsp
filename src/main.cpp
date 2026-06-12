#include "hw/board.h"

#include "platform/filesystem.h"

#include <ESPAsyncWebServer.h>
#ifndef DISABLE_OTA
#include <ElegantOTA.h>
#endif

#include "platform/pwm.h"

#ifdef ESP32
//  https://github.com/espressif/arduino-esp32/issues/863#issuecomment-347179737
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

#include <AceButton.h>
#include "Version.h"
#include "connect.h"
#include "hw/statusLed.h"
#include "artnetHandler.h"
#include "api.h"
#include "config.h"
#include "device/device.h"
#include "device/relay.h"
#ifndef SONOFF_BASIC
#include "device/dimmer.h"
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
        if (config.dmx.size() > 0 && dmx_devices[0])
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

// WiFi credentials, device config and the web UI all live on the
// filesystem - nothing loop() does (button, Art-Net, devices, WiFi, web
// server) can work without it. Blink LED_PIN and log forever instead of
// returning into a loop() that would run against never-initialized
// config/devices/network.
void safeMode(const String &reason)
{
    pinMode(LED_PIN, OUTPUT);
    while (true)
    {
        LOG("FATAL: " + reason + " - halted in safe mode");
        for (int i = 0; i < 10; i++)
        {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(50);
        }
    }
}

void setup()
{
    Serial.begin(115200);
    unsigned long serialWaitStart = millis();
    while (!Serial && (millis() - serialWaitStart < 2000))
    {
        delay(10);
    }
    LOG(F("Starting..."));
#if defined(ESP8266)
    if (!ESP_FS.begin())
#else
    // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
    if (!ESP_FS.begin(true))
#endif
    {
        safeMode(F("Cannot init the filesystem"));
    };
    delay(100);

    config.load();
    if (config.host.length())
    {
        // hostname(const String&) is the portable spelling: ESP8266's
        // LwipIntf and ESP32's WiFiGenericClass (-> setHostname) both
        // implement it with the same signature/return type.
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
    Pwm::init(config.hardware.pwmFreq);

    // config.cpp clamps config.dmx to MAX_DMX_DEVICES at load time; reuse that
    // bound everywhere dmx_devices[MAX_DMX_DEVICES] is indexed or its size is
    // handed off, so a stale/oversized config can't cause an OOB read (B8).
    uint8_t deviceCount = config.dmx.size() < MAX_DMX_DEVICES ? config.dmx.size() : MAX_DMX_DEVICES;

    for (int i = 0; i < deviceCount; i++)
    {
        dmx_devices[i] = NULL;
        switch (config.dmx[i]->type)
        {
        case art::DmxType::Relay:
            dmx_devices[i] = new DmxRelay(1, config.dmx[i]->channel, config.dmx[i]->pin, config.dmx[i]->level, config.dmx[i]->threshold);
            break;
#ifndef SONOFF_BASIC
        case art::DmxType::Servo:
            dmx_devices[i] = new DmxServo(1, config.dmx[i]->channel, config.dmx[i]->pin);
            break;
        case art::DmxType::Dimmer:
            dmx_devices[i] = new PwmDimmer(1, config.dmx[i]->channel, config.dmx[i]->pin, config.dmx[i]->pulse, config.dmx[i]->multiplier, config.dmx[i]->level);
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
    for (int i = 0; i < deviceCount; i++)
    {
        longName += " " + config.dmxTypeToString(config.dmx[i]->type) + " #" + String(config.dmx[i]->channel);
    }
    LOG("ArtNet Node: " + config.host + " : " + longName);
    artnet.init(config.universe, config.host, longName, dmx_devices, deviceCount);

    // Setup WWW
    server.reset();

#ifndef DISABLE_OTA
    ElegantOTA.begin(&server);
#endif

    setupApi(&server, config, &connect);
    server.begin(); // Call ONLY AFTER WiFi gets connected !!!!! (or reboot loop)

    LOG(F("BOOT OK v" VERSION)); // deterministic success marker for tools/boot_check.py
}

void loop()
{
    // B11: apply any config staged by POST /config here, on the main task,
    // before touching config.dmx/config.wifi below.
    config.applyPendingUpdate();
    button.check();
    artnet.loop();
    for (int i = 0; i < config.dmx.size() && i < MAX_DMX_DEVICES; i++)
        if (dmx_devices[i])
            dmx_devices[i]->handle();

#ifndef DISABLE_OTA
    ElegantOTA.loop();
#endif
    connect.loop();
#ifdef OLED_SSD1306
    statusDisplay->loop();
#endif
}