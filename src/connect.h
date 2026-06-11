#ifndef CONNECT_H
#define CONNECT_H

#if defined(ESP8266)
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h> //https://github.com/tzapu/WiFiManager

#include "hw/logger.h"
#include "hw/statusLed.h"

extern StatusLed *status;

class Connect
{
protected:
    AsyncWiFiManager *wifiManager;

    static const unsigned long REFRESH_INTERVAL = 1000; // ms
    unsigned long lastRefreshTime;
    String hostName;
    // Tracks WiFi connection state across loop() ticks so the
    // lost/reconnected transitions are logged once, not every
    // second (B12).
    bool wifiConnected = true;

public:
    void init(AsyncWebServer *server, DNSServer *dns);
    void connect(String hostName);
    void loop();
    void reset();
};

#endif