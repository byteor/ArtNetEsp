#ifndef DIMMER_H
#define DIMMER_H

#include <Arduino.h>
#include "device.h"
#include "boards/board.h"
#include "hw/logger.h"
#include "platform/pwm.h"

// default strobe pulse length, ms
#define DEFAULT_STROBE_PULSE 5

class PwmDimmer : public Device
{
protected:
    uint8_t pin;
    int pulse = 0;  // 'active' duration, ms
    int period = 0; // 'total' duration, ms
    bool enabled;
    bool isFlipped;
    uint8_t activeState;
    uint8_t inactiveState;
    int state;     // 1/0 == ON/OFF
    int value = 0; // original PWM value, 0-255
    int multiplier;
    int adjustedActiveValue = 0;   // PWM value adjusted to the 'active' level
    int adjustedInactiveValue = 0; // PWM value adjusted to the 'active' level
    int adjustedMaxValue;          // Max active PWM value adjusted to the 'active' level
    int valueOverride;             // manually (literally!) set PWM value adjusted to the 'active' level
    unsigned long previousMillis = 0;
    unsigned long interval;

public:
    PwmDimmer(uint8_t universe, uint16_t channel, uint8_t pin = LED_BUILTIN, int pulse = DEFAULT_STROBE_PULSE, int multiplier = 1, int activeState = HIGH);
    void start() override;
    void start(uint8_t value);
    void stop() override;
    void flip() override;
    void update();
    bool isEnabled() override;
    uint8_t get(uint16_t channel) override;
    void set(uint16_t channel, uint8_t data) override;
    uint16_t getNumberOfChannels() override { return 1; } // Temporarily set to 1

    void setInterval(int millis);
    void setDuration(int millis);
    void setPin(int number);
    void handle() override;
};

#endif