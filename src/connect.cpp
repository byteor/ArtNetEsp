#include "connect.h"

// gets called when WiFiManager enters configuration mode
void configModeCallback(AsyncWiFiManager *myWiFiManager)
{
    status->set(StatusLed::CaptivePortal);
    LOG("Entered config mode");
    LOG(WiFi.softAPIP());
    // if you used auto generated SSID, print it
    LOG(myWiFiManager->getConfigPortalSSID());
}

void Connect::init(AsyncWebServer *server, DNSServer *dns)
{
    wifiManager = new AsyncWiFiManager(server, dns);
    // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode (Captive Portal)
    wifiManager->setAPCallback(configModeCallback);
}

void Connect::connect(String hostName)
{
    status->set(StatusLed::Connecting);
    this->hostName = hostName;
    if (!wifiManager->autoConnect(hostName.c_str()))
    {
        LOG(F("failed to connect and hit timeout"));
        ESP.restart();
        delay(1000);
    }
    LOG("Connected :)");
    LOG(WiFi.localIP());
    wifiConnected = true;
    status->set(StatusLed::Connected);
}

void Connect::reset()
{
    LOG(F("Resetting WiFi settings :)"));
#ifdef ESP32
    WiFi.disconnect(true, true);
#else
    WiFi.disconnect(true);
#endif
    delay(100);
}

void Connect::loop()
{
    if (millis() - lastRefreshTime > REFRESH_INTERVAL)
    {
        lastRefreshTime = millis();
        if (!WiFi.isConnected())
        {
            if (wifiConnected)
            {
                wifiConnected = false;
                LOG(F("WiFi connection lost, reconnecting in background"));
                status->set(StatusLed::Connecting);
            }
            // Non-blocking (B12): ask the WiFi stack to retry with the
            // last-used credentials and return immediately, so loop()
            // keeps running devices/Art-Net/etc while WiFi is down.
            // Unlike connect()'s autoConnect(), this never opens the
            // captive portal - that stays a cold-boot-only behavior.
            WiFi.reconnect();
        }
        else if (!wifiConnected)
        {
            wifiConnected = true;
            LOG(F("WiFi reconnected"));
            LOG(WiFi.localIP());
            status->set(StatusLed::Connected);
        }
    }
}