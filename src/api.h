#pragma once

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include "connect.h"
#include <Ticker.h>
#include "hw/logger.h"
#include "config.h"

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

void setupApi(AsyncWebServer *server, art::Config &config, Connect *connect)
{
    server->on("/heap", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(200, "text/plain", String(ESP.getFreeHeap())); });

    server->serveStatic("/", ESP_FS, "/www/").setDefaultFile("index.html");

    server->on("/config", HTTP_GET, [&](AsyncWebServerRequest *request)
               {
                    LOG("GET /config");
                    String json;
                    config.serialize(json);
                    LOG(json);
                    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
                    request->send(response); });

    // POST /config
    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/config", [&](AsyncWebServerRequest *request, JsonVariant &json)
                                                                           {
        LOG("PUT /config");

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

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"reboot\":\"OK\"}");
        request->send(response);
        apiTicker.once_ms(200, restart); });

    // POST /reset-wifi
    server->on("/reset-wifi", HTTP_POST, [connect](AsyncWebServerRequest *request)
               {
        LOG("POST /reset-wifi");

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"reset\":\"OK\"}");
        request->send(response);
        apiTicker.once_ms(200, disconnectAndRestart, connect); });

    server->onNotFound([](AsyncWebServerRequest *request)
                       { request->send(404, "text/plain", "Not found"); });

    server->addHandler(handler);
}