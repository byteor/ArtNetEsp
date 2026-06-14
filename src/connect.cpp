#include "connect.h"

#include "Version.h"

namespace
{
const byte DNS_PORT = 53;
}

void Connect::init(AsyncWebServer *server, DNSServer *dns, StatusLed *status)
{
    this->server = server;
    this->dns = dns;
    this->status = status;
}

bool Connect::tryStationConnect(unsigned long timeoutMs)
{
    WiFi.mode(WIFI_STA);
    // No-arg begin() retries whatever credentials are already saved in the
    // WiFi stack's own NVS - the same store a previous portal submission
    // (or ESPAsyncWiFiManager, before this) would have written via
    // WiFi.begin(ssid, pass)'s default WiFi.persistent(true).
    WiFi.begin();

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs)
    {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

String Connect::portalPage()
{
    // Build the <option> list from a blocking scan - acceptable here, nothing
    // else is running while the portal is up (connect() blocks on
    // credentialsReceived).
    String options;
    int found = WiFi.scanNetworks();
    for (int i = 0; i < found; i++)
    {
        String ssid = WiFi.SSID(i);
        // Minimal escaping so an SSID with quotes/angle-brackets/ampersands
        // can't break out of the <option> it's dropped into.
        ssid.replace("&", "&amp;");
        ssid.replace("\"", "&quot;");
        ssid.replace("<", "&lt;");
        options += "<option value=\"" + ssid + "\">" + ssid + "</option>";
    }
    WiFi.scanDelete();

    // Serve the styled page from LittleFS (data/www/portal.html), filling in
    // the device name, version, AP IP, and scanned networks.
    File f = ESP_FS.open(PORTAL_PAGE_PATH, "r");
    if (!f)
    {
        // Fallback: firmware flashed without `pio run -t uploadfs`, so the
        // static page isn't on the filesystem. Serve a minimal inline page
        // so WiFi setup still works.
        LOG(F("portal.html missing - serving minimal fallback page"));
        String html = F("<!DOCTYPE html><html><head><meta name=\"viewport\" "
                        "content=\"width=device-width,initial-scale=1\"><title>");
        html += hostName;
        html += F(" - WiFi Setup</title></head><body><h1>");
        html += hostName;
        html += F(" - WiFi Setup</h1><form method=\"POST\" action=\"/\">"
                  "<input name=\"ssid\" placeholder=\"Network (SSID)\" required><br><br>"
                  "<input name=\"pass\" type=\"password\" placeholder=\"Password\"><br><br>"
                  "<input type=\"submit\" value=\"Connect\"></form></body></html>");
        return html;
    }

    String html = f.readString();
    f.close();

    html.replace("{{HOST}}", hostName);
    html.replace("{{VERSION}}", VERSION);
    html.replace("{{IP}}", WiFi.softAPIP().toString());
    html.replace("{{NETWORKS}}", options);
    return html;
}

void Connect::startPortal()
{
    status->set(StatusLed::CaptivePortal);

    // AP_STA (not just AP) so WiFi.scanNetworks() in portalPage() works
    // while the AP is up.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(hostName.c_str());
    IPAddress apIP = WiFi.softAPIP();

    LOG(F("Entered config mode"));
    LOG(apIP);
    LOG(hostName);

    dns->start(DNS_PORT, "*", apIP);

    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(200, "text/html", portalPage()); });

    // portalPage() links /style.css (the same stylesheet the home page uses);
    // serve it straight from LittleFS. Without this route the onNotFound
    // captive-portal redirect below would bounce it back to "/" and the page
    // would render unstyled.
    server->on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(ESP_FS, "/www/style.css", "text/css"); });

    server->on("/", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
        if (!request->hasParam("ssid", true))
        {
            request->send(400, "text/plain", "Missing ssid");
            return;
        }
        newSsid = request->getParam("ssid", true)->value();
        // The portal page's <select> offers an "Other network..." option
        // (value "__other__") that reveals a free-text SSID field; honor it.
        if (newSsid == "__other__")
        {
            newSsid = request->hasParam("ssid_manual", true) ? request->getParam("ssid_manual", true)->value() : "";
        }
        newPass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
        request->send(200, "text/html",
                      F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                        "<title>Connecting...</title><style>"
                        "body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
                        "background:#eef1f6;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Arial,sans-serif;color:#1f2937}"
                        "div{max-width:340px;text-align:center;background:#fff;padding:32px 26px;border-radius:16px;margin:16px}"
                        "h1{margin:0 0 10px;font-size:20px;color:#1d4ed8}p{margin:0;font-size:14px;color:#6b7280;line-height:1.5}"
                        "</style></head><body><div><h1>Connecting...</h1>"
                        "<p>The device is rebooting and joining your network. If it doesn't come back, "
                        "rejoin this device's WiFi to retry the setup page.</p></div></body></html>"));
        credentialsReceived = true; });

    // Captive-portal detection: send every other request to the setup page.
    server->onNotFound([apIP](AsyncWebServerRequest *request)
                        {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
        response->addHeader("Location", "http://" + apIP.toString() + "/");
        request->send(response); });

    server->begin();
}

void Connect::connect(String hostName)
{
    status->set(StatusLed::Connecting);
    this->hostName = hostName;

    bool forcePortal = ESP_FS.exists(FORCE_PORTAL_PATH);
    if (forcePortal)
    {
        ESP_FS.remove(FORCE_PORTAL_PATH);
        LOG(F("WiFi reset requested - starting setup portal"));
    }
    else if (tryStationConnect(STA_CONNECT_TIMEOUT_MS))
    {
        LOG("Connected :)");
        LOG(WiFi.localIP());
        wifiConnected = true;
        status->set(StatusLed::Connected);
        return;
    }
    else
    {
        LOG(F("No saved WiFi connection - starting setup portal"));
    }

    startPortal();

    while (!credentialsReceived)
    {
        dns->processNextRequest();
        delay(10);
    }

    dns->stop();
    server->end();
    WiFi.softAPdisconnect(true);

    LOG("Trying new network: " + newSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(newSsid.c_str(), newPass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < PORTAL_CONNECT_TIMEOUT_MS)
    {
        delay(250);
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        LOG("Connected :)");
        LOG(WiFi.localIP());
    }
    else
    {
        LOG(F("Failed to connect to the new network - rebooting to retry"));
    }

    // Either way: reboot. On success, setup() continues normally next boot;
    // on failure, tryStationConnect() times out again and the portal
    // reopens - matches the previous wifiManager->autoConnect() behavior.
    ESP.restart();
    delay(1000);
}

void Connect::reset()
{
    LOG(F("Resetting WiFi settings :)"));

    // Belt and suspenders for B29: a LittleFS write+close is synchronous
    // (unlike WiFi.disconnect(true, true)'s NVS erase, which may not
    // complete before ESP.restart()), so this reliably forces the setup
    // portal on the next boot regardless of whether the WiFi stack's saved
    // credentials actually got erased.
    File f = ESP_FS.open(FORCE_PORTAL_PATH, "w");
    if (f)
    {
        f.close();
    }

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
            // Unlike connect()'s portal path, this never opens the
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
