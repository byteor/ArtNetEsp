# ESP8266 ArtNet Node

## TODO:

- [ ] Root HTML - reset button
- [ ] Root HTML - templates showing the host name and version
- [ ] Root HTML - CSS
- [ ] BUG: Reboot doesn't always work correctly on Sonoff Basic
- [ ] Document all known implementations
- [ ] DMX: support multiple universes
- [ ] DMX: single-channel Dimmer (new type)
- [ ] DMX: NeoPixel strip (new type)
- [ ] Strobe: Flip
- [ ] Strobe: Stroboscope
- [x] ArtNet: BUG: Repeater skips broadcasts
- [ ] ArtNet: Discovery
- [ ] Status LED: active LOH/HIGH setting 
- [x] BUG: only one DMX config works
- [x] Support ESP32
## Version History
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
