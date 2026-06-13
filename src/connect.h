#ifndef CONNECT_H
#define CONNECT_H

#include "platform/filesystem.h"

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h> //https://github.com/tzapu/WiFiManager

#include "hw/logger.h"
#include "hw/statusLed.h"

class Connect
{
protected:
    AsyncWiFiManager *wifiManager;
    StatusLed *status;

    static const unsigned long REFRESH_INTERVAL = 1000; // ms
    unsigned long lastRefreshTime;
    String hostName;
    // Tracks WiFi connection state across loop() ticks so the
    // lost/reconnected transitions are logged once, not every
    // second (B12).
    bool wifiConnected = true;

public:
    // status outlives this Connect (owned by App) - Connect only observes it.
    void init(AsyncWebServer *server, DNSServer *dns, StatusLed *status);
    void connect(String hostName);
    void loop();
    void reset();
};

#endif