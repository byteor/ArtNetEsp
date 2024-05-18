#ifdef OLED_SSD1306
/**
 * SSD1306 OLED Display 128x64
 * Default I2C pins
*/
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Ticker.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif
#include "config.h"

class StatusDisplay
{
protected:
    const float TICK_DURATION = 0.5f;
    const int TICKS_PER_PAGE = 2;
    const int TICKS_PER_MESSAGE = 4;
    const int MAIN_PAGES = 2; // assuming no DMX devices are setup
    int PAGES = MAIN_PAGES;

    Adafruit_SSD1306 *display = NULL;

    Ticker _ticker;
    art::Config *config;
    int page = 0, ticksForPage = 0, ticksForMessage = 0;
    char buf[128];

    void drawStr(uint8_t x, uint8_t y, const char *string)
    {
        drawStr(x, y, string, 1);
    }

    void drawStr(uint8_t x, uint8_t y, const char *string, uint8_t size)
    {
        if (!display)
            return;
        display->setTextSize(size); // Normal 1:1 pixel scale
        display->setCursor(x, y);
        display->write(string);
    }

    void setStatus32();
    void setStatus64();

public:
    void setConfig(art::Config *config)
    {
        this->config = config;
        PAGES += config->dmx.size();
        setStatus();
    }

    void setStatus()
    {
        if (!display)
            return;
        if (ticksForMessage-- > 0)
            return;
        ticksForMessage = 0;

        display->clearDisplay();

        if (ticksForPage++ >= TICKS_PER_PAGE)
        {
            page++;
            ticksForPage = 0;
        }
        if (page >= PAGES)
        {
            page = 0;
        }
#if SCREEN_HEIGHT >= 64
        setStatus64();
#else
        setStatus32();
#endif
        display->display();
    }

    void message(String text)
    {
        if (!display)
            return;
        display->setFont();
        ticksForMessage = TICKS_PER_MESSAGE;
        display->clearDisplay();
        drawStr(0, 0, text.c_str());
        display->display();
    }

    static void staticTick(StatusDisplay *sd)
    {
        sd->setStatus();
    }

    StatusDisplay()
    {
        display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
        display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
        display->setTextSize(1);              // Normal 1:1 pixel scale
        display->setTextColor(SSD1306_WHITE); // Draw white text
#if SCREEN_HEIGHT >= 64
        display->setFont(&FreeSans9pt7b);
#endif
        display->clearDisplay();
        display->display();
        _ticker.attach_scheduled(TICK_DURATION, std::bind(&StatusDisplay::setStatus, this));
    }
};

void StatusDisplay::setStatus32()
{
    display->setFont();
    if (WiFi.status() == WL_CONNECTED)
    {
        switch (page)
        {
        case 0:
            drawStr(0, 8, config->host.c_str(), 2);
            break;
        case 1:
            drawStr(0, 0, WiFi.localIP().toString().c_str());
            drawStr(0, 16, WiFi.SSID().c_str());
            sprintf(buf, "%d dBm", WiFi.RSSI());
            drawStr(48, 16, buf);
            //drawStr(0, 60, WiFi.hostname().c_str());
            break;
        default:
            // DMX channel page
            int i = page - MAIN_PAGES;

            sprintf(buf, "%d", config->dmx[i]->channel);
            drawStr(0, 8, buf, 2);
            //drawStr(0, 16, "DMX:");
            sprintf(buf, "%d/%d  %s", i + 1, config->dmx.size(), config->dmxTypeToString(config->dmx[i]->type).c_str());
            drawStr(48, 0, buf);
            drawStr(48, 16, "i/o:");
            sprintf(buf, "%d", config->dmx[i]->pin);
            drawStr(80, 16, buf);
        }
    }
    else
    {
        drawStr(64, 0, PSTR("No WiFi"));
        drawStr(0, 16, WiFi.hostname().c_str(), 2);
    }
}

void StatusDisplay::setStatus64()
{
    display->setFont(&FreeSans9pt7b);
    if (WiFi.status() == WL_CONNECTED)
    {
        switch (page)
        {
        case 0:
            drawStr(0, 32, config->host.c_str());
            break;
        case 1:
            drawStr(0, 14, WiFi.localIP().toString().c_str());
            drawStr(0, 40, WiFi.SSID().c_str());
            sprintf(buf, "%d dBm", WiFi.RSSI());
            drawStr(48, 62, buf);
            //drawStr(0, 60, WiFi.hostname().c_str());
            break;
        default:
            // DMX channel page
            int i = page - MAIN_PAGES;

            sprintf(buf, "%d", config->dmx[i]->channel);
            drawStr(0, 60, buf, 2);
            //drawStr(0, 16, "DMX:");
            sprintf(buf, "%d/%d  %s", i + 1, config->dmx.size(), config->dmxTypeToString(config->dmx[i]->type).c_str());
            drawStr(0, 14, buf);
            drawStr(80, 40, "i/o:");
            sprintf(buf, "%d", config->dmx[i]->pin);
            drawStr(84, 62, buf);
        }
    }
    else
    {
        drawStr(24, 16, PSTR("No WiFi"));
        drawStr(0, 48, WiFi.hostname().c_str());
    }
}

#endif