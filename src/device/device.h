#ifndef DEVICE_H
#define DEVICE_H

#define DMX_SILENCE_TIMEOUT 5000

class Device
{
protected:
    uint8_t universe;
    uint16_t channel;
    unsigned long lastChange = 0;

public:
    virtual ~Device() = default;

    virtual void start() {}; // allocate resources, init
    virtual void stop() {};  // deallocate resources
    virtual void flip() {};  // call to flip the state (depending on device type)
    virtual void set(uint16_t channel, uint8_t data) {};
    virtual uint8_t get(uint16_t channel) { return 0; }
    virtual bool isEnabled() { return true; }
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
    uint16_t getChannel() { return channel; }
    virtual uint16_t getNumberOfChannels() = 0;
};

#endif // DEVICE_H