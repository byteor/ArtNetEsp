#ifndef CONNECT_H
#define CONNECT_H

#if defined(ESP8266)
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif

//#define USE_ESP_WIFIMANAGER_NTP     false
//#define _ESPASYNC_WIFIMGR_LOGLEVEL_    3

//needed for library
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h> //https://github.com/tzapu/WiFiManager
//#include <ESPAsync_WiFiManager.h>

#include "logger.h"
#include "statusLed.h"

extern StatusLed* status;

class Connect
{
protected:
    AsyncWiFiManager *wifiManager;

    static const unsigned long REFRESH_INTERVAL = 1000; // ms
    static unsigned long lastRefreshTime;

public:
    void init(AsyncWebServer *server, DNSServer *dns);
    void connect(String hostName);
    static void loop(Connect *connect);
    void reset();
};

#endif