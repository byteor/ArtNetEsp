#include "safeMode.h"

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <ESPAsyncWebServer.h>

#include "boards/board.h"
#include "hw/logger.h"

namespace
{
// Static storage duration - only ever constructed/used from within
// safeMode()'s infinite loop, never alongside App's own `server`.
AsyncWebServer safeServer(80);

String diagnosticPage(const String &reason)
{
    String html = F("<!DOCTYPE html><html><head><title>ArtNet - Safe Mode</title></head><body>");
    html += F("<h1>ArtNet Node - Safe Mode</h1>");
    html += "<p><b>Chip ID:</b> " + CHIP_ID + "</p>";
    html += "<p><b>Reason:</b> " + reason + "</p>";
    html += F("<p>The filesystem could not be mounted, so the device configuration, "
              "WiFi credentials, and the normal web UI are all unavailable.</p>");
    html += F("<p>To recover, reflash both the firmware and the filesystem image, then re-apply "
              "any DMX/device configuration via <code>POST /config</code> once the device "
              "reconnects:</p>");
    html += F("<pre>pio run -t upload\npio run -t uploadfs</pre>");
    html += F("</body></html>");
    return html;
}
} // namespace

void safeMode(const String &reason)
{
    pinMode(LED_PIN, OUTPUT);

    // No captive-portal/ESPAsyncWiFiManager here - its assets live on the
    // filesystem we just failed to mount. A fixed, open diagnostic AP at
    // the usual 192.168.4.1 is the only thing that can work.
    String ssid = "ArtNet-" + CHIP_ID + "-SAFE";
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str());
    LOG("Safe mode AP: " + ssid + " @ " + WiFi.softAPIP().toString());

    safeServer.onNotFound([reason](AsyncWebServerRequest *request)
                           { request->send(200, "text/html", diagnosticPage(reason)); });
    safeServer.begin();

    // The diagnostic page is a best-effort addition - if WiFi/AsyncWebServer
    // themselves were affected by whatever broke the filesystem, this loop
    // (and LED_PIN) remain the last-resort signal.
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
