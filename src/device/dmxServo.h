#ifndef SONOFF_BASIC

#include <Arduino.h>
#ifdef ESP32
#include <ESP32Servo.h>
#else
#include <Servo.h>
#endif
#include "device.h"

class DmxServo : public Device
{
    uint8_t pin;
    int state; // 1/0 == ON/OFF
    int value; // original PWM value, 0-255
    Servo servo;
    uint8_t data;

public:
    void start();
    void stop();
    void flip();
    uint8_t get(uint8_t channel);
    void set(uint8_t channel, uint8_t data);
    uint16_t getNumberOfChannels() { return 1; }

    DmxServo(uint8_t universe, uint8_t channel, uint8_t pin);
};

DmxServo::DmxServo(uint8_t universe, uint8_t channel, uint8_t pin)
{
    this->universe = universe;
    this->channel = channel;
    this->pin = pin;
    this->state = 0;
    this->value = 90;
    Serial.print("Servo on pin: ");
    Serial.print(pin);
    Serial.print(" DMX channel: ");
    Serial.print(channel);
    Serial.print(" Universe: ");
    Serial.println(universe);
}

uint8_t DmxServo::get(uint8_t channel)
{
    return data;
}

void DmxServo::set(uint8_t dmxChannel, uint8_t data)
{
    /*
    The goal is to translate DMX value [0...255] to a value acceptable by Servo
    which may depend on the servo model
    For FT90R it is 90 +- 20, i.e. [60...120]
    0, 255, 127 = stop
    0..63 - increasing ?clockwise?, 63 is the max speed
    64..127 - decreasing
    128..191 - increasing, 191 is the max speed
    192..255 - decreasing
    */
    if (dmxChannel == channel)
    {
        this->data = data;
        int dX = 0;
        float dY = 30;
        float d0 = 90;
        float b = 0;
        int section = 256 / 4;
        float m = dY / (float)section;
        if (data < section)
        {
            m *= -1;
            b = d0;
            dX = 0;
        }
        else if (data >= section * 3)
        {
            m *= -1;
            b = d0 + dY;
            dX = -section * 3;
        }
        else
        {
            dX = -section;
            b = d0 - dY;
        }
        value = (float)(data + dX) * m + b;

        Serial.print("Servo: ");
        Serial.println(value);
        if (servo.attached())
        {
            servo.write(value);
        }
    }
}

void DmxServo::start()
{
    if (!servo.attached())
    {
        servo.attach(pin);
        state = 1;
        servo.write(value);
    }
}

void DmxServo::stop()
{
    servo.detach();
    state = 0;
}

void DmxServo::flip()
{
    // do nothing
}

#endif // SONOFF_BASIC