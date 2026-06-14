#include "app.h"

#include "boards/board.h"
#include "platform/filesystem.h"
#include "platform/mdns.h"
#include "platform/netbios.h"
#include "platform/pwm.h"

#ifndef DISABLE_OTA
#include <ElegantOTA.h>
#endif

#ifdef ESP32
//  https://github.com/espressif/arduino-esp32/issues/863#issuecomment-347179737
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

#include "Version.h"
#include "net/webApi.h"
#include "app/deviceFactory.h"
#include "app/safeMode.h"

using namespace ace_button;

void App::setup()
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
    // NB: the DHCP hostname is applied in Connect (after WiFi.mode(WIFI_STA),
    // before WiFi.begin()) - setting it here, before WiFi is brought up, is
    // unreliable on ESP32. See Connect::applyHostname().
    status = std::make_unique<StatusLed>(config.hardware.ledPin, LOW);

    // Buttons
    // Just a single button. Long press (5s) causes a hard reset of WiFi settings and reboots into a Captive Portal
    buttonConfig.setLongPressDelay(config.hardware.longPressDelay);
    buttonConfig.setFeature(ButtonConfig::kFeatureLongPress);
    button.init(&buttonConfig, config.hardware.buttonPin);
    pinMode(config.hardware.buttonPin, INPUT_PULLUP);
    buttonConfig.setIEventHandler(this);

    LOG("Button on pin: " + String(config.hardware.buttonPin) + " long press is: " + String(config.hardware.longPressDelay) + " ms");

#if FEATURE_OLED
    statusDisplay = std::make_unique<StatusDisplay>();
    statusDisplay->setConfig(&config);
#endif

    // Devices
    Pwm::init(config.hardware.pwmFreq);

    // config.cpp clamps config.dmx to MAX_DMX_DEVICES at load time; reuse that
    // bound everywhere devices[MAX_DMX_DEVICES] is indexed or its size is
    // handed off, so a stale/oversized config can't cause an OOB read (B8).
    deviceCount = config.dmx.size() < MAX_DMX_DEVICES ? config.dmx.size() : MAX_DMX_DEVICES;

    for (int i = 0; i < deviceCount; i++)
    {
        devices[i] = app::makeDevice(config.dmx[i], 1);
        if (devices[i])
        {
            devices[i]->setBlackout(config.dmx[i].blackout);
            devices[i]->start();
        }
        else
        {
            LOG(F("Incompatible DMX device type:"));
            LOG(static_cast<int>(config.dmx[i].type));
        }
        deviceList[i] = devices[i].get();
    }

    LOG("Init WiFi...");
    connect.init(&server, &dnsServer, status.get());
    connect.connect(config.host);

    if (config.host.length())
    {
        platform::mdnsBegin(config.host);
        platform::netbiosBegin(config.host);
    }

    // ArtNet
    String longName = config.host;
    for (int i = 0; i < deviceCount; i++)
    {
        longName += " " + config.dmxTypeToString(config.dmx[i].type) + " #" + String(config.dmx[i].channel);
    }
    LOG("ArtNet Node: " + config.host + " : " + longName);
    artnet.init(config.universe, config.host, longName, deviceList, deviceCount);

    // Setup WWW
    server.reset();

#ifndef DISABLE_OTA
    ElegantOTA.begin(&server);
    if (config.hardware.authEnabled)
    {
        ElegantOTA.setAuth(config.hardware.authUser.c_str(), config.hardware.authPass.c_str());
    }
#endif

    webApi::setup(&server, config, &connect);
    // WiFi must be connected (connect.connect() above) before server.begin() -
    // otherwise the AsyncWebServer/AsyncTCP stack ends up in a reboot loop.
    server.begin();

    LOG(F("BOOT OK v" VERSION)); // deterministic success marker for tools/boot_check.py
}

void App::loop()
{
    // B11: apply any config staged by POST /config here, on the main task,
    // before touching config.dmx/config.wifi below.
    config.applyPendingUpdate();
    button.check();
    artnet.loop();
    for (int i = 0; i < deviceCount; i++)
        if (devices[i])
            devices[i]->tick();

#ifndef DISABLE_OTA
    ElegantOTA.loop();
#endif
    connect.loop();
    platform::mdnsLoop();
#if FEATURE_OLED
    statusDisplay->loop();
#endif
}

void App::handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
    switch (eventType)
    {
    case AceButton::kEventLongPressed:
        LOG(F("Hard reset caused by a Button"));
        connect.reset();
        ESP.restart();
        break;
    case AceButton::kEventPressed:
#if FEATURE_OLED
        statusDisplay->message("v." + String(VERSION) + String("\nHold for 5 seconds\nto reset WiFi"));
#endif
        LOG(F("Button pressed"));
        for (int i = 0; i < deviceCount; i++)
        {
            if (devices[i])
            {
                devices[i]->flip();
            }
        }
        if (deviceCount > 0 && devices[0])
        {
            if (devices[0]->isEnabled())
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
