#include "connect.h"

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
    String html = F("<!DOCTYPE html><html><head><title>");
    html += hostName;
    html += F(" - WiFi Setup</title></head><body>");
    html += "<h1>" + hostName + " - WiFi Setup</h1>";
    html += F("<p>Pick (or type) the WiFi network this device should join, "
              "enter its password, and submit. The device will reboot and "
              "try to connect - if it fails, this setup page reappears.</p>");
    html += F("<form method=\"POST\" action=\"/\">");
    html += F("<label>Network (SSID):</label><br>");
    html += F("<input name=\"ssid\" list=\"ssids\" required><br>");
    html += F("<datalist id=\"ssids\">");

    // Blocking scan - acceptable here, nothing else is running while the
    // portal is up (connect() blocks on credentialsReceived).
    int found = WiFi.scanNetworks();
    for (int i = 0; i < found; i++)
    {
        html += "<option value=\"" + WiFi.SSID(i) + "\">";
    }
    WiFi.scanDelete();

    html += F("</datalist><br>");
    html += F("<label>Password:</label><br>");
    html += F("<input name=\"pass\" type=\"password\"><br><br>");
    html += F("<input type=\"submit\" value=\"Connect\">");
    html += F("</form></body></html>");
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

    server->on("/", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
        if (!request->hasParam("ssid", true))
        {
            request->send(400, "text/plain", "Missing ssid");
            return;
        }
        newSsid = request->getParam("ssid", true)->value();
        newPass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
        request->send(200, "text/html", F("<!DOCTYPE html><html><body><h1>Connecting...</h1>"
                                           "<p>Rebooting - rejoin this device's WiFi to retry "
                                           "the setup page if it doesn't come back.</p></body></html>"));
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
