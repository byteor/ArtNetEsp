#ifndef NET_WEB_API_H
#define NET_WEB_API_H

#include <ESPAsyncWebServer.h>

#include "config.h"
#include "connect.h"

// REST API: GET /heap, static /www/, GET/POST /config, GET /status,
// POST /reboot, POST /reset-wifi, 404 handler. Replaces the old api.h
// (Phase 6 item 3) - also where GET /config's "info" block (runtime
// WiFi/build/chip identity, never part of the persisted config) now lives.
namespace webApi
{
void setup(AsyncWebServer *server, art::Config &config, Connect *connect);
} // namespace webApi

#endif // NET_WEB_API_H
