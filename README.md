# ESP8266 ArtNet Node

## Device Types

### DIMMER

TBD

### RELAY

TBD

### SERVO

TBD

### REPEATER

TBD

## Controls

### Indicator LED

TBD

### Button

TBD

### OLED Screen

TBD

## WiFi connection and Captive Portal

On startup, if a known WiFi network is not available or WiFi was never successfully connected before, Captive Portal starts and
is accessible via default IP address: [192.168.4.1](192.168.4.1)

If a known network comes back to life while the Captive Portal is active, the device will be automatically connected to it.

WiFi connection is constantly being checked every second. If connection is lost, a reconnect attempt will be made following the same logic as at startup.

## Variations

### Sonoff Basic

- [x] OTA is disabled
- [x] Only RELAY device type is supported

## REST API

### GET /config

Response example:

```json
{
  "_needReboot": false, //reboot needed flag indicating config changes were not applied yet
  "hw": {
    "freq": 600, // PWM frequency
    "ledPin": 2, // GPIO pin cooected to indicator LED
    "buttonPin": 0, // GPIO pin connected to button
    "longPressDelay": 5000 // Duration in ms to cause a 'factory reset'
  },
  "info": {
    "version": "2021.4",
    "ssid": "BAM",
    "rssi": -47
  },
  "id": "d6b6b8", // Chip ID
  "host": "GREEN-d6d8", // Host name (used in ArtNet discovery)
  "dmx": [
    // An array of virtual DMX devices (up to 4)
    {
      "channel": 8, // DMX channel
      "type": "DIMMER", // Device type - DIMMER | RELAY | SERVO | REPEATER
      "pin": 5, // Output pin
      "level": "HIGH", // Output pin 'active' level - LOW | HIGH
      "threshold": 127 // A level at such RELAY changes its state
    }
  ]
}
```

### PUT /config

Payload example (see response example for details):

```json
{
  "hw": {
    "freq": 600,
    "ledPin": 2,
    "buttonPin": 0,
    "longPressDelay": 5000
  },
  "host": "BLACK",
  "dmx": [
    {
      "channel": 9,
      "type": "DIMMER",
      "pin": 14,
      "level": "LOW"
    }
  ]
}
```

it is not required to send the whole payload, it can be partial, missing elements are ignored:

```json
{
  "dmx": [
    {
      "channel": 9
    }
  ]
}
```

### Important notes:

- After changes have been made, a reboot required to apply them
- `dmx` is an array since one device may implement multiple functions. Elements are not named but index beased, therefore **all** `dmx` elements have to be present with any update to `dmx` collection!

### POST /reboot

Makes a device to reboot. No payload required

### POST /reset-wifi

Resets WiFi settings and reboots

### GET /heap

Returns the heap size

## OTA

OTA is supported via [http://<DEVICE_IP>/update](http://<DEVICE_IP>/update) URL

---

## TODO:

- [ ] Root HTML - reset button
- [ ] Root HTML - templates showing the host name and version
- [ ] Root HTML - CSS
- [ ] BUG: Reboot doesn't always work correctly on Sonoff Basic
- [ ] Document all known implementations
- [x] DMX: support multiple universes (one per physical device)
- [ ] DMX: single-channel Dimmer (new type)
- [ ] DMX: NeoPixel strip (new type)
- [x] Strobe: Flip
- [ ] Strobe: Fix Stroboscope (or remove stroboscope at all)
- [x] ArtNet: BUG: Repeater skips broadcasts
- [x] ArtNet: Discovery
- [x] BUG: only one DMX config works
- [x] Support ESP32
- [x] Reconnect on WiFi disconnects
- [ ] Make blackout on DMX timeout optional
- [ ] Rename Strobe class (it is meaningless)

## Version History

### 2024.1

- Updated dependencies
- new DMX library
- ArtNet discovery
- Reconnect on WiFi disconnect
- Universe support

### 2021.4

- FIXED: multiple DMX channels actually work
- DIMMER: button enables/disables -> On/Off
- Support ESP32 (not field tested yet)

### 2021.3

- FIXED: Repeater skips broadcasts. AsyncUDP rules! But this comes with a high price of dropping ArtNet Discovery feature
- FIXED: OTA was not working because of wrong memory allocation

### 2021.2

- DMX Gateway
- OTA updates (not for boards with 1M flash)
- Relay - temporary flip - switch the On/Off state on the button press
- default HTML page - pointing to config and BitBucket
- correct PlatformIO settings for LittleFS
- OLED - support both 128x64 and 128x32 resolutions

### 2021.1

- Servo
- Dimmer
- Relay
- OLED support
- Long press to reset

---
