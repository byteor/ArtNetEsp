#pragma once

// Seeed XIAO ESP32-S3 + RS485 DMX repeater + I2S OLED (128x64)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET -1

#define LED_PIN 21 // XIAO built-in LED pin
#define DEFAULT_BUTTON_PIN 0

#define DMX_TX_PIN 3
#define DMX_RX_PIN 12    // not used, required by esp_dmx
#define DMX_ENABLE_PIN 13 // not used, required by esp_dmx
#define DMX_PORT 1
