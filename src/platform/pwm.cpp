#include "platform/pwm.h"
#include "hw/logger.h"

#ifdef ESP32

// ESP32-S2/S3 only expose 8 LEDC channels; cap the pool at the smallest
// common value so the same code works on every ESP32 variant.
#define PWM_MAX_CHANNELS 8
#define PWM_RESOLUTION_BITS 8

static uint16_t s_freq = 1000;
static uint8_t s_pins[PWM_MAX_CHANNELS];
static uint8_t s_numChannels = 0;

void Pwm::init(uint16_t freq)
{
    s_freq = freq;
}

void Pwm::write(uint8_t pin, uint8_t value)
{
    bool attached = false;
    for (uint8_t i = 0; i < s_numChannels; i++)
    {
        if (s_pins[i] == pin)
        {
            attached = true;
            break;
        }
    }
    if (!attached)
    {
        if (s_numChannels >= PWM_MAX_CHANNELS)
        {
            LOG(F("Pwm: no free LEDC channels"));
            return;
        }
        s_pins[s_numChannels++] = pin;
        ledcAttach(pin, s_freq, PWM_RESOLUTION_BITS);
    }
    ledcWrite(pin, value);
}

#else

void Pwm::init(uint16_t freq)
{
    analogWriteFreq(freq);
    analogWriteRange(255);
}

void Pwm::write(uint8_t pin, uint8_t value)
{
    analogWrite(pin, value);
}

#endif
