#pragma once

// Lolin S2 Mini + RS485 DMX repeater
//
// DMX pins were previously inherited from hw/board.h's #ifndef fallback
// defaults with no board-specific decision (AGENTS Gotcha #3) - written
// down explicitly here, same values as before (17/16/21/1).

#define LED_PIN LED_BUILTIN
#define DEFAULT_BUTTON_PIN 0

#define DMX_TX_PIN 17
#define DMX_RX_PIN 16
#define DMX_ENABLE_PIN 21
#define DMX_PORT 1
