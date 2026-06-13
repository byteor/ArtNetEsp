#include "boards/features.h"

#if FEATURE_DIMMER

#ifndef DIMMER_H
#define DIMMER_H

#include <Arduino.h>
#include "device.h"
#include "boards/board.h"
#include "hw/logger.h"
#include "platform/pwm.h"
#include "core/dimmerLogic.h"

// default strobe pulse length, ms
#define DEFAULT_STROBE_PULSE 5

class PwmDimmer : public Device
{
protected:
    uint8_t pin;
    int pulse;     // configured 'active' duration, ms (reapplied on each Strobe-channel update)
    int multiplier;
    uint8_t activeState;
    uint8_t inactiveState;
    core::DimmerLogic logic; // pure timing/value state machine - src/core/dimmerLogic.h

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
    uint16_t channelCount() override { return 1; } // Temporarily set to 1

    void setInterval(int millis);
    void setDuration(int millis);
    void setPin(int number);
    void tick() override;
};

#endif // DIMMER_H

#endif // FEATURE_DIMMER