#pragma once

/**
 * Feature flags derived from the BOARD_<NAME> macro (see boards/board.h).
 * FEATURE_DMX_PORT/FEATURE_SERVO/FEATURE_DIMMER are 0 on the relay-only
 * Sonoff boards; FEATURE_OLED is 1 on the three boards with an SSD1306.
 */

#if defined(BOARD_SONOFF_BASIC) || defined(BOARD_SONOFF_S31)
#define FEATURE_DMX_PORT 0
#define FEATURE_SERVO 0
#define FEATURE_DIMMER 0
#else
#define FEATURE_DMX_PORT 1
#define FEATURE_SERVO 1
#define FEATURE_DIMMER 1
#endif

#if defined(BOARD_D1_MINI_OLED) || defined(BOARD_HELTEC_WIFI_KIT_8) || defined(BOARD_SEEED_XIAO_ESP32S3_OLED)
#define FEATURE_OLED 1
#else
#define FEATURE_OLED 0
#endif
