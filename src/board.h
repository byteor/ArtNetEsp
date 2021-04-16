/**
 * Defaults for different boards
*/
#include <Arduino.h>

#ifndef LED_PIN
#define LED_PIN LED_BUILTIN
#endif

#ifndef SCREEN_ADDRESS
#define SCREEN_ADDRESS 0x3C
#endif

#ifdef ARDUINO_ESP8266_HELTEC_WIFI_KIT_8
  //Serial.println("ARDUINO_ESP8266_HELTEC_WIFI_KIT_8");
    // 0.91" OLED display
    #define SCREEN_WIDTH 128 // OLED display width, in pixels
    #define SCREEN_HEIGHT 32 // OLED display height, in pixels
    #define SCREEN_ADDRESS 0x3C
    #define OLED_RESET 16
#else
    // Generic 0.96" OLED display
    #define SCREEN_WIDTH 128 // OLED display width, in pixels
    #define SCREEN_HEIGHT 64 // OLED display height, in pixels
    #define OLED_RESET -1
#endif

// Button is on GPIO 0 by default
#define DEFAULT_BUTTON_PIN 0
// Long Press delay - to reset WiFi settings [s]
#define DEFAULT_BUTTON_LONG_PRESS 5
// default PWM frequency for dimmers
#define DEFAULT_PWM_FREQ 600
// ===

#ifdef ESP32
#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
#else
#define ESP_getChipId()   (ESP.getChipId())
#endif
#define CHIP_ID       (String(ESP_getChipId(), HEX)) 