#ifndef StatusLed_h
#define StatusLed_h

#include <Ticker.h>
#include <Arduino.h>
#ifdef ESP32
#include <analogWrite.h>
#endif
#include "logger.h"

class StatusLed
{
    uint8_t _ledPin;
    uint8_t _onValue;
    Ticker _ticker;

    static constexpr float STATUS_CONNECTING = 0.1f;
    static constexpr float STATUS_PORTAL = 0.5f;
    // 8-bit analogWrite duty range: ESP8266 via analogWriteRange(255) in
    // main.cpp, ESP32 "ESP32 AnalogWrite" library's default valueMax.
    static constexpr uint8_t PWM_MAX = 255;
    static constexpr uint8_t STATUS_BRIGHTNESS_OFF = 0;
    static constexpr uint8_t STATUS_BRIGHTNESS_ON = 55; // dim "solid" glow - README's "Solid dimmed" normal-operation pattern

    // analogWrite duty is time spent at logic HIGH. For an active-LOW LED
    // (_onValue == LOW), more HIGH time means dimmer, so invert the duty.
    void setBrightness(uint8_t brightness)
    {
        analogWrite(_ledPin, _onValue == HIGH ? brightness : PWM_MAX - brightness);
    }

public:
    enum Status
    {
        Connecting,
        CaptivePortal,
        Connected,
        ON,
        OFF
    };

    StatusLed(uint8_t pin, uint8_t onValue)
    {
        _ledPin = pin;
        _onValue = onValue;
        pinMode(pin, OUTPUT);
    }

    void tick()
    {
        digitalWrite(_ledPin, !digitalRead(_ledPin));
    }

    static void staticTick(StatusLed *sl)
    {
        sl->tick();
    }

    void set(Status status)
    {
        LOG("Change Status to");
        LOG(status);

        switch (status)
        {
        case Status::Connecting:
            _ticker.attach(STATUS_CONNECTING, &staticTick, this);
            break;
        case Status::CaptivePortal:
            _ticker.attach(STATUS_PORTAL, &staticTick, this);
            break;
        case Status::Connected:
        case Status::ON:
            stop(true);
            setBrightness(STATUS_BRIGHTNESS_ON);
            break;
        case Status::OFF:
            setBrightness(STATUS_BRIGHTNESS_OFF);
            break;
        default:
            stop(false);
            break;
        }
    }

    void stop(bool keepOn)
    {
        _ticker.detach();
        digitalWrite(_ledPin, keepOn ? _onValue : !_onValue);
    }
};

#endif
