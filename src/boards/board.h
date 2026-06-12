#pragma once

/**
 * Board dispatch: platformio.ini defines exactly one BOARD_<NAME> macro
 * per environment; pull in that board's pin/geometry header, then the
 * handful of defaults shared by every board.
 */
#include <Arduino.h>

#include "Version.h"

#ifndef SONOFF_BASIC
#include <Wire.h>
#include <SPI.h>
#endif

#if defined(BOARD_D1_MINI_OLED)
#include "boards/d1_mini_oled.h"
#elif defined(BOARD_D1_MINI)
#include "boards/d1_mini.h"
#elif defined(BOARD_NODEMCUV2)
#include "boards/nodemcuv2.h"
#elif defined(BOARD_HELTEC_WIFI_KIT_8)
#include "boards/heltec_wifi_kit_8.h"
#elif defined(BOARD_SONOFF_BASIC)
#include "boards/sonoff_basic.h"
#elif defined(BOARD_SONOFF_S31)
#include "boards/sonoff_s31.h"
#elif defined(BOARD_LOLIN32)
#include "boards/lolin32.h"
#elif defined(BOARD_ESP32_DEVKITC_V4)
#include "boards/esp32-devkitc-v4.h"
#elif defined(BOARD_LOLIN_S2_MINI)
#include "boards/lolin_s2_mini.h"
#elif defined(BOARD_ESP32_S3_DEVKITC_1)
#include "boards/esp32-s3-devkitc-1.h"
#elif defined(BOARD_SEEED_XIAO_ESP32S3)
#include "boards/seeed_xiao_esp32s3.h"
#elif defined(BOARD_SEEED_XIAO_ESP32S3_OLED)
#include "boards/seeed_xiao_esp32s3_oled.h"
#else
#error "No BOARD_<NAME> defined - add -D BOARD_<NAME> to this environment in platformio.ini"
#endif

// Long Press delay - to reset WiFi settings [s]
#define DEFAULT_BUTTON_LONG_PRESS 5
// default PWM frequency for dimmers
#define DEFAULT_PWM_FREQ 600

#ifdef ESP32
#define ESP_getChipId() ((uint32_t)ESP.getEfuseMac())
#define CHIP_ARC "ESP32"
#else
#define ESP_getChipId() (ESP.getChipId())
#define CHIP_ARC "ESP8266"
#endif
#define CHIP_ID (String(ESP_getChipId(), HEX))
