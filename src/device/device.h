#ifndef DEVICE_H
#define DEVICE_H

#define DMX_SILENCE_TIMEOUT 5000

class Device
{
protected:
    uint8_t universe;
    uint8_t channel;
    unsigned long lastChange;

public:
    virtual void start() {}; // allocate resources, init
    virtual void stop() {};  // deallocate resources
    virtual void flip() {};  // call to flip the state (depending on device type)
    virtual void set(uint8_t channel, uint8_t data) {};
    virtual void handle() // call it from loop() if needed
    {
        if (millis() - lastChange > DMX_SILENCE_TIMEOUT)
        {
            Serial.println(F("DMX timeout"));
            frame();
            set(channel, 0);
        }
    }
    virtual void frame()
    {
        lastChange = millis();
    }
    virtual void frame(const uint32_t univ, const uint8_t *data, const uint16_t size)
    {
        lastChange = millis();
    }
    uint8_t getUniverse() { return universe; }
    uint8_t getChannel() { return channel; }
    virtual uint16_t getNumberOfChannels();
};

#endif // DEVICE_H