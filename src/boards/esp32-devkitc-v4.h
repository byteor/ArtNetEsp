#pragma once

// ESP32 DevKitC v4 + RS485 DMX repeater

#define LED_BUILTIN 2
#define LED_PIN 14
#define DEFAULT_BUTTON_PIN 27

#define DMX_TX_PIN 26
#define DMX_RX_PIN 12    // not used, required by esp_dmx
#define DMX_ENABLE_PIN 13 // not used, required by esp_dmx
#define DMX_PORT 1
