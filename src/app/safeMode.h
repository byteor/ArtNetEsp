#ifndef APP_SAFE_MODE_H
#define APP_SAFE_MODE_H

#include <Arduino.h>

// WiFi credentials, device config and the web UI all live on the
// filesystem - nothing loop() does (button, Art-Net, devices, WiFi, web
// server) can work without it. Blink LED_PIN and log forever instead of
// returning into a loop() that would run against never-initialized
// config/devices/network.
void safeMode(const String &reason);

#endif // APP_SAFE_MODE_H
