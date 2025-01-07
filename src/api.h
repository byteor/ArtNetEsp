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

    // POST /reboot
    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/config", [&](AsyncWebServerRequest *request, JsonVariant &json)
                                                                           {
        LOG("PUT /config");

        if (config.update(json))
        {
            config.save();
        }
        else
        {
            LOG("Reloading existing config");
            config.load();
        }

        String jsonString;
        config.serialize(jsonString);
        LOG(jsonString);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonString);
        request->send(response); });

    // POST /reboot
    server->on("/reboot", HTTP_POST, [&](AsyncWebServerRequest *request)
               {
        LOG("POST /reboot");

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"reboot\":\"OK\"}");
        request->send(response);
        apiTicker.once_ms(200, restart); });

    // POST /reset-wifi
    server->on("/reset-wifi", HTTP_POST, [&](AsyncWebServerRequest *request)
               {
        LOG("POST /reset-wifi");

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"reset\":\"OK\"}");
        request->send(response);
        apiTicker.once_ms(200, disconnectAndRestart, connect); });

    server->onNotFound([](AsyncWebServerRequest *request)
                       { request->send(404, "text/plain", "Not found"); });

    server->addHandler(handler);
}