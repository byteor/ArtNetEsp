#pragma once

// Platform-free (no Arduino.h, no millis(), no Pwm/LOG) - the pure
// strobe/dimmer timing + value-adjust state machine extracted from
// device/dimmer.cpp (PwmDimmer). Safe to include from native tests.
//
// Time is passed explicitly to tick() rather than read via millis(), so
// this class has no notion of "now" of its own.

#include <cstdint>

namespace core
{

class DimmerLogic
{
public:
    // activeState: 1 (HIGH) or 0 (LOW) - matches Arduino's HIGH/LOW
    // constants, i.e. which digital level corresponds to "on".
    explicit DimmerLogic(uint8_t activeState)
        : activeState(activeState),
          inactiveState(activeState ? 0 : 1),
          state(activeState),
          adjustedMaxValue(activeState ? 255 : 0),
          valueOverride(activeState ? 0 : 255)
    {
    }

    // Dimmer channel: DMX 0-255 -> adjusted duty values for the active and
    // inactive states, and the new valueOverride used while active.
    void setValue(uint8_t data)
    {
        value = data;
        adjustedActiveValue = activeState ? value : 255 - value;
        adjustedInactiveValue = activeState ? 0 : 255;
        valueOverride = adjustedActiveValue;
    }

    // Strobe channel: 'active' duration (ms). Negative clamps to 0.
    void setDuration(int millis)
    {
        pulse = millis;
        if (pulse < 0)
            pulse = 0;
    }

    // Strobe channel: 'total' duration (ms). Negative resets to pulse
    // (i.e. no strobing - period <= pulse keeps the output steady-on).
    void setInterval(int millis)
    {
        period = millis;
        if (period < 0)
            period = pulse;
    }

    // Toggle enabled. B18: flipping ON while no DMX value has been
    // received yet (value == 0) brings the light to full
    // (adjustedMaxValue), not to the (zero) adjusted DMX value.
    void flip()
    {
        enabled = !enabled;
        if (enabled)
            valueOverride = value > 0 ? adjustedActiveValue : adjustedMaxValue;
        else
            valueOverride = adjustedInactiveValue;
    }

    // Re-arm the strobe state machine and enable output (PwmDimmer::start()).
    void start()
    {
        interval = 0;
        state = activeState;
        enabled = true;
    }

    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }

    // Advance the strobe/period state machine to nowMs. Returns true if
    // duty() may have changed as a result.
    bool tick(unsigned long nowMs)
    {
        if (period <= pulse)
        {
            previousMillis = nowMs;
            if (state != activeState)
            {
                state = activeState;
                return true;
            }
            return false;
        }

        if (nowMs - previousMillis >= static_cast<unsigned long>(interval))
        {
            previousMillis = nowMs;
            if (state == activeState)
            {
                state = inactiveState;
                interval = period - pulse;
            }
            else
            {
                state = activeState;
                interval = pulse;
            }
            return true;
        }
        return false;
    }

    // Current PWM duty (0-255) for the configured pin.
    uint8_t duty() const
    {
        if (enabled && state == activeState)
            return static_cast<uint8_t>(valueOverride);
        return static_cast<uint8_t>(adjustedInactiveValue);
    }

    uint8_t getValue() const { return value; }

private:
    uint8_t activeState;
    uint8_t inactiveState;
    bool enabled = true;
    int pulse = 0;
    int period = 0;
    int interval = 0;
    int state;
    uint8_t value = 0;
    int adjustedActiveValue = 0;
    int adjustedInactiveValue = 0;
    int adjustedMaxValue;
    int valueOverride;
    unsigned long previousMillis = 0;
};

} // namespace core
