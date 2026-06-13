#ifndef APP_APP_H
#define APP_APP_H

#include <array>
#include <memory>

#include <AceButton.h>
#include <ESPAsyncWebServer.h>

#include "boards/features.h"
#include "config.h"
#include "connect.h"
#include "device/device.h"
#include "hw/statusLed.h"
#include "net/artnetService.h"

#if FEATURE_OLED
#include "hw/oledDisplay.h"
#endif

// Owns every global the firmware needs - the web server, WiFi/captive
// portal, Art-Net service, configured DMX devices, button, and (where
// present) the OLED status display - and encodes the boot order between
// them. main.cpp is just `App app; app.setup(); app.loop();`.
class App : public ace_button::IEventHandler
{
public:
    void setup();
    void loop();

    // ace_button::IEventHandler - handles button presses (flip configured
    // devices) and long-presses (WiFi reset).
    void handleEvent(ace_button::AceButton *button, uint8_t eventType, uint8_t buttonState) override;

private:
    AsyncWebServer server{80};
    DNSServer dnsServer;
    Connect connect;
    art::Config config;
    ArtnetService artnet;
    std::unique_ptr<StatusLed> status;

    ace_button::ButtonConfig buttonConfig;
    ace_button::AceButton button;

    // Owning storage for the configured DMX devices.
    std::array<std::unique_ptr<Device>, MAX_DMX_DEVICES> devices;
    // Raw view of `devices` for ArtnetService::init's Device** API - the
    // array itself is a member (not a local) because ArtnetService keeps
    // and dereferences the pointer for the lifetime of the program.
    Device *deviceList[MAX_DMX_DEVICES] = {nullptr};
    uint8_t deviceCount = 0;

#if FEATURE_OLED
    std::unique_ptr<StatusDisplay> statusDisplay;
#endif
};

#endif // APP_APP_H
