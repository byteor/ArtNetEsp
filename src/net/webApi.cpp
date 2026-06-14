#include "net/webApi.h"

#include <AsyncJson.h>
#include <Ticker.h>

#include "Version.h"
#include "boards/board.h"
#include "hw/logger.h"

namespace webApi
{

namespace
{
Ticker apiTicker;

void restart()
{
    ESP.restart();
}

void disconnectAndRestart(Connect *cn)
{
    cn->reset();
    ESP.restart();
}

// Optional HTTP basic-auth (config.hardware.authEnabled), guarding mutating
// routes (POST /config, /reboot, /reset-wifi) and, separately, ElegantOTA's
// /update via ElegantOTA.setAuth() (see app/app.cpp). GET routes (/config,
// /status, /heap, static /www/) stay unauthenticated - read-only, theater LAN.
// Returns true if the request may proceed; otherwise sends the 401 challenge.
bool checkAuth(AsyncWebServerRequest *request, art::Config &config)
{
    if (!config.hardware.authEnabled)
        return true;
    if (request->authenticate(config.hardware.authUser.c_str(), config.hardware.authPass.c_str()))
        return true;
    request->requestAuthentication();
    return false;
}

// Runtime WiFi/build/chip identity - never part of the persisted config
// (§1.2.9 layering fix), populated fresh on every request. Shared by
// GET /config (nested under "info", for back-compat) and GET /status (the
// object itself).
void fillInfo(JsonObject info)
{
    info["id"] = CHIP_ID;
    info["chip"] = CHIP_ARC;
    info["version"] = VERSION;
    info["built"] = BUILD_TIMESTAMP;
    info["max_dmx_devices"] = MAX_DMX_DEVICES;
    info["ssid"] = WiFi.SSID();
    info["rssi"] = WiFi.RSSI();
    info["uptime"] = millis();
    info["free_heap"] = ESP.getFreeHeap();
    // Whether ElegantOTA's /update endpoint is compiled in (stripped on the
    // sonoff envs via DISABLE_OTA, from boards/board.h). Lets the web UI hide
    // its OTA link on boards that don't have it.
#ifdef DISABLE_OTA
    info["ota"] = false;
#else
    info["ota"] = true;
#endif
}
} // namespace

void setup(AsyncWebServer *server, art::Config &config, Connect *connect)
{
    server->on("/heap", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(200, "text/plain", String(ESP.getFreeHeap())); });

    server->serveStatic("/", ESP_FS, "/www/").setDefaultFile("index.html");

    server->on("/config", HTTP_GET, [&](AsyncWebServerRequest *request)
               {
                    LOG("GET /config");
                    JsonDocument doc;
                    config.toJson(doc);
                    fillInfo(doc["info"].to<JsonObject>());
                    String json;
                    serializeJsonPretty(doc, json);
                    LOG(json);
                    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
                    request->send(response); });

    // GET /status: a lightweight poll endpoint with the runtime "info" fields
    // (no dmx[]/wifi[] arrays) plus the _needReboot flag, so the web UI's
    // periodic poll can clear its "reboot required" banner once the device has
    // actually rebooted (_dirty resets to false on boot).
    server->on("/status", HTTP_GET, [&config](AsyncWebServerRequest *request)
               {
                    LOG("GET /status");
                    JsonDocument doc;
                    fillInfo(doc.to<JsonObject>());
                    doc["_needReboot"] = config.needsReboot();
                    String json;
                    serializeJson(doc, json);
                    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
                    request->send(response); });

    // POST /config
    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/config", [&](AsyncWebServerRequest *request, JsonVariant &json)
                                                                           {
        LOG("PUT /config");

        if (!checkAuth(request, config))
            return;

        // B11: this lambda runs in the async_tcp task on ESP32, while loop()
        // (main task) concurrently reads config.dmx/config.wifi (device
        // handle(), button handler, StatusDisplay). Stage the JSON here and
        // let loop() call applyPendingUpdate(), so update()/cleanupDmx()/
        // cleanupWiFi()/save() run single-threaded on the same task as those
        // readers.
        AsyncWebServerResponse *response;
        if (config.stageUpdate(json))
        {
            response = request->beginResponse(202, "application/json", "{\"status\":\"pending\"}");
        }
        else
        {
            response = request->beginResponse(500, "application/json", "{\"error\":\"update too large\"}");
        }
        request->send(response); });

    // POST /reboot
    server->on("/reboot", HTTP_POST, [&](AsyncWebServerRequest *request)
               {
        LOG("POST /reboot");

        if (!checkAuth(request, config))
            return;

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"reboot\":\"OK\"}");
        request->send(response);
        apiTicker.once_ms(200, restart); });

    // POST /reset-wifi
    server->on("/reset-wifi", HTTP_POST, [&, connect](AsyncWebServerRequest *request)
               {
        LOG("POST /reset-wifi");

        if (!checkAuth(request, config))
            return;

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"reset\":\"OK\"}");
        request->send(response);
        apiTicker.once_ms(200, disconnectAndRestart, connect); });

    server->onNotFound([](AsyncWebServerRequest *request)
                       { request->send(404, "text/plain", "Not found"); });

    server->addHandler(handler);
}

} // namespace webApi
