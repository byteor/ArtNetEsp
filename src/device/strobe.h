#ifndef STROBE_H
#define STROBE_H

#include <Arduino.h>
#ifdef ESP32
#include <analogWrite.h>
#endif
#include "device.h"
#include "hw/board.h"
#include "hw/logger.h"

// default strobe pulse length, ms
#define DEFAULT_STROBE_PULSE 5

class Strobe : public Device
{
protected:
    uint8_t pin;
    int pulse = 0;  // 'active' duration, ms
    int period = 0; // 'total' duration, ms
    bool enabled;
    bool isFlipped;
    uint8_t activeState;
    uint8_t inactiveState;
    int state; // 1/0 == ON/OFF
    int value; // original PWM value, 0-255
    int multiplier;
    int adjustedActiveValue;   // PWM value adjusted to the 'active' level
    int adjustedInactiveValue; // PWM value adjusted to the 'active' level
    int adjustedMaxValue;      // Max active PWM value adjusted to the 'active' level
    int valueOverride;         // manually (literally!) set PWM value adjusted to the 'active' level
    unsigned long previousMillis = 0;
    unsigned long interval;

public:
    Strobe(uint8_t universe, uint8_t channel, uint8_t pin = LED_BUILTIN, int pulse = DEFAULT_STROBE_PULSE, int multiplier = 1, int activeState = HIGH);
    void start();
    void start(uint8_t value);
    void stop();
    void flip();
    void update();
    bool isEnabled();
    uint8_t get(uint8_t channel);
    void set(uint8_t channel, uint8_t data);
    uint16_t getNumberOfChannels() { return 1; } // Temporarily set to 1

    void setInterval(int millis);
    void setDuration(int millis);
    void setPin(int number);
    void handle() override;
};

#endif