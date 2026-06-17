#pragma once

// Seeed XIAO ESP32-S3 + RS485 DMX repeater (assets/Xiao-Repeater.jpg)

#define LED_PIN 21 // XIAO built-in LED pin
#define DEFAULT_BUTTON_PIN 0

#define DMX_TX_PIN 3
#define DMX_RX_PIN 12    // not used, required by esp_dmx
#define DMX_ENABLE_PIN 13 // not used, required by esp_dmx
#define DMX_PORT 1
