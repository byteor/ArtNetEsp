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
    static constexpr int STATUS_PWM_OFF = 1000;
    static constexpr int STATUS_PWM_ON = 200;

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
            analogWrite(_ledPin, STATUS_PWM_ON);
            break;
        case Status::OFF:
            analogWrite(_ledPin, STATUS_PWM_OFF);
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
