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

#ifdef ESP32

#endif

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
int getRotationalValue(uint8_t data)
{
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
    return (float)(data + dX) * m + b;
}

/*
The goal is to translate DMX value [0...255] to a value acceptable by Servo
MG90 is a 180 degree servo
Having MIN, MAX, and OFFSET we can translate DMX value to a degree value even servo is 360 degrees
*/
int getAngleValue(uint8_t data)
{
    double MIN = 501, MAX = 2500; // 179;
    double range = MAX - MIN;
    double x = data;
    return x * range / 255 + MIN;
}

void DmxServo::set(uint8_t dmxChannel, uint8_t data)
{

    if (dmxChannel == channel)
    {
        value = getAngleValue(data);
        Serial.print("Servo: ");
        Serial.println(value);
        if (servo.attached())
        {
            servo.writeMicroseconds(value);
        }
        else
        {
            Serial.print("Servo not attached");
        }
    }
}

void DmxServo::start()
{
    if (!servo.attached())
    {
        servo.attach(pin, 500, 2500);
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