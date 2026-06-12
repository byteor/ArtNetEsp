#ifndef PLATFORM_PWM_H
#define PLATFORM_PWM_H

#include <Arduino.h>

// Cross-platform 8-bit PWM output (0-255 duty cycle).
// ESP8266: analogWrite(), with the global frequency/range set by init().
// ESP32: LEDC, with one channel lazily allocated per pin on its first
// write() call, using the frequency passed to init() (replaces
// erropix/ESP32 AnalogWrite, which left config.hardware.pwmFreq unused).
class Pwm
{
public:
    static void init(uint16_t freq);
    static void write(uint8_t pin, uint8_t value);
};

#endif
