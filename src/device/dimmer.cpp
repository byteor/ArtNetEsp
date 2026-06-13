#include "boards/features.h"

#if FEATURE_DIMMER

#include "dimmer.h"

PwmDimmer::PwmDimmer(uint8_t universe, uint16_t channel, uint8_t pin, int pulse, int multiplier, int activeState)
    : logic(activeState == HIGH ? 1 : 0)
{
    LOG("New Dimmer: pin=" + String(pin) + " DMX channel:" + String(channel));

    this->universe = universe;
    this->channel = channel;
    this->pin = pin;
    this->pulse = pulse;
    this->multiplier = multiplier;
    this->activeState = activeState;
    this->inactiveState = activeState == HIGH ? LOW : HIGH;

    logic.setDuration(pulse);

    pinMode(pin, OUTPUT);
    digitalWrite(pin, inactiveState);
}

uint8_t PwmDimmer::get(uint16_t channel)
{
    return logic.getValue();
}

void PwmDimmer::set(uint16_t dmxChannel, uint8_t data)
{
    if (dmxChannel == channel) // Dimmer
    {
        LOG_DEBUG("Dimmer: " + String(dmxChannel) + " = " + String(data));
        logic.setValue(data);
        update();
    }
}

void PwmDimmer::onDmx(uint32_t univ, const uint8_t *data, uint16_t len)
{
    frame(univ, data, len);

    if (len > 0 && data[0] != logic.getValue())
    {
        LOG_DEBUG("Dimmer: " + String(channel) + " = " + String(data[0]));
        logic.setValue(data[0]);
        update();
    }

    // B17 fix: channel+1 is the strobe speed, finally reachable now that
    // channelCount()==2 makes ArtnetService include it in the slice.
    if (len > 1)
    {
        LOG_DEBUG("Dimmer Strobe: " + String(channel + 1) + " = " + String(data[1]));
        logic.setDuration(pulse);
        logic.setInterval(data[1] * multiplier);
    }
}

void PwmDimmer::update()
{
    uint8_t duty = logic.duty();
    Pwm::write(pin, duty);
    LOG(" =" + String(duty));
}

void PwmDimmer::setInterval(int millis)
{
    logic.setInterval(millis);
}

void PwmDimmer::setDuration(int millis)
{
    pulse = millis;
    if (pulse < 0)
        pulse = 0;
    logic.setDuration(pulse);
}

void PwmDimmer::setPin(int number)
{
    if (pin != number)
    {
        pinMode(pin, INPUT);
        pin = number;
        pinMode(pin, OUTPUT);
        digitalWrite(pin, inactiveState);
    }
}

void PwmDimmer::tick()
{
    if (!logic.isEnabled())
        return;
    unsigned long currentMillis = millis();
    if (lastChange != 0 && millis() - lastChange > DMX_SILENCE_TIMEOUT)
    {
        LOG(F("DMX Silence Timeout"));
        set(channel, 0);
        lastChange = millis();
    }
    else if (logic.tick(currentMillis))
    {
        update();
    }
}

void PwmDimmer::start()
{
    if (!logic.isEnabled())
    {
        LOG(F("Dimmer Started"));
        logic.start();
        lastChange = millis();
    }
}

void PwmDimmer::stop()
{
    logic.setEnabled(false);
    digitalWrite(pin, inactiveState);
}

void PwmDimmer::flip()
{
    logic.flip();
    if (logic.isEnabled())
    {
        LOG(F("Flip - ON"));
    }
    else
    {
        LOG(F("Flip - OFF"));
    }
    update();
}

bool PwmDimmer::isEnabled()
{
    return logic.isEnabled();
}

#endif // FEATURE_DIMMER
