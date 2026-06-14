#ifndef CONNECT_H
#define CONNECT_H

#include "platform/filesystem.h"

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include "hw/logger.h"
#include "hw/statusLed.h"

class Connect
{
protected:
    AsyncWebServer *server;
    DNSServer *dns;
    StatusLed *status;

    static const unsigned long REFRESH_INTERVAL = 1000; // ms
    // Cold-boot: how long to wait for WiFi.begin() (NVS-saved credentials)
    // before giving up and opening the setup portal.
    static const unsigned long STA_CONNECT_TIMEOUT_MS = 10000;
    // Portal: how long to wait for newly-submitted credentials to connect
    // before rebooting (and re-opening the portal if they were wrong).
    static const unsigned long PORTAL_CONNECT_TIMEOUT_MS = 15000;

    // Written by reset() and checked by connect() on the next boot - forces
    // the setup portal even if the WiFi stack's NVS-saved credentials are
    // still (or again) connectable. This sidesteps relying on
    // WiFi.disconnect(true, true)'s NVS-erase completing before ESP.restart()
    // (B29) - a filesystem write/close is synchronous, unlike that erase.
    static constexpr const char *FORCE_PORTAL_PATH = "/force_portal.flag";

    // Styled captive-portal page, flashed to LittleFS via `pio run -t uploadfs`
    // (data/www/portal.html). portalPage() reads it and substitutes the
    // {{HOST}}/{{VERSION}}/{{IP}}/{{NETWORKS}} placeholders; if it's missing
    // (firmware flashed without uploadfs), a minimal inline page is served.
    static constexpr const char *PORTAL_PAGE_PATH = "/www/portal.html";

    unsigned long lastRefreshTime;
    String hostName;
    // Tracks WiFi connection state across loop() ticks so the
    // lost/reconnected transitions are logged once, not every
    // second (B12).
    bool wifiConnected = true;

    // Set by the portal's POST handler once the user submits credentials;
    // connect() blocks on this while servicing DNS requests.
    volatile bool credentialsReceived = false;
    String newSsid;
    String newPass;

    // Tries WiFi.begin() with whatever credentials are already stored in
    // the WiFi stack's NVS, waiting up to timeoutMs.
    bool tryStationConnect(unsigned long timeoutMs);
    // Brings up an open AP + DNS wildcard redirect + a minimal setup page
    // on `server`, and registers the handlers that set credentialsReceived.
    void startPortal();
    String portalPage();

public:
    // server/dns/status outlive this Connect (owned by App) - Connect only
    // observes/uses them.
    void init(AsyncWebServer *server, DNSServer *dns, StatusLed *status);
    void connect(String hostName);
    void loop();
    void reset();
};

#endif
