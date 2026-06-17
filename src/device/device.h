#ifndef DEVICE_H
#define DEVICE_H

#define DMX_SILENCE_TIMEOUT 5000

class Device
{
protected:
    uint8_t universe;
    uint16_t channel;
    unsigned long lastChange = 0;
    bool silenceLogged = false;     // B20: log "DMX timeout" once per silence period, not every 5s
    bool manualOverride = false;    // B20: set by flip(); suspends the silence blackout until the next real frame()
    bool blackoutOnSilence = true;  // Phase 5 item 5: policy hook for tick()'s set(channel,0) on silence; wired to config by item 9

public:
    virtual ~Device() = default;

    virtual void start() {}; // allocate resources, init
    virtual void stop() {};  // deallocate resources
    virtual void flip() {};  // call to flip the state (depending on device type)
    virtual void set(uint16_t channel, uint8_t data) {};
    virtual uint8_t get(uint16_t channel) { return 0; }
    virtual bool isEnabled() { return true; }
    virtual void tick() // call it from loop() if needed
    {
        if (millis() - lastChange > DMX_SILENCE_TIMEOUT)
        {
            if (!silenceLogged)
            {
                Serial.println(F("DMX timeout"));
                silenceLogged = true;
            }
            // Reset lastChange (not silenceLogged) so this repeats every
            // DMX_SILENCE_TIMEOUT without re-logging until a real frame arrives.
            frame();
            if (!manualOverride && blackoutOnSilence)
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
        silenceLogged = false;
        manualOverride = false;
    }

    // Device interface v2 (Phase 5 item 6): ArtnetService passes a SLICE of
    // the incoming frame already offset to this device's firstChannel() -
    // data[0] is channel firstChannel(), data[i] is channel firstChannel()+i.
    // ArtnetService guarantees len>0 only when firstChannel() >= 1, so no
    // underflow/`< 1` guard is needed here.
    virtual void onDmx(uint32_t univ, const uint8_t *data, uint16_t len)
    {
        frame(univ, data, len);
        uint16_t fc = firstChannel();
        uint16_t cc = channelCount();
        for (uint16_t i = 0; i < len && i < cc; i++)
        {
            uint16_t ch = fc + i;
            if (data[i] != get(ch))
                set(ch, data[i]);
        }
    }

    uint8_t getUniverse() { return universe; }
    uint16_t getChannel() { return channel; }
    virtual uint16_t firstChannel() { return channel; }
    virtual uint16_t channelCount() = 0;
    void setBlackout(bool enabled) { blackoutOnSilence = enabled; }
};

#endif // DEVICE_H