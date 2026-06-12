#pragma once

// Heltec WiFi Kit 8 - built-in 0.91" SSD1306 OLED (128x32)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET 16

// This board has no LED on the usual LED_BUILTIN pin; GPIO2 (D4) is used instead
#define LED_BUILTIN 2
#define LED_PIN LED_BUILTIN
#define DEFAULT_BUTTON_PIN 0
