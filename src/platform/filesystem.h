#ifndef PLATFORM_FILESYSTEM_H
#define PLATFORM_FILESYSTEM_H

#if defined(ESP8266)
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <FS.h>
#include <LittleFS.h>
#define ESP_FS LittleFS
#else
#include <WiFi.h>
#include <LittleFS.h>
#define ESP_FS LittleFS
#endif

#endif
