# XIAOESP32S3 Art-Net to DMX Gateway

A wireless DMX gateway turned out to be the most useful application of this firmware. It is dirt cheap and works really well with both ESP8266 and ESP32 boards.

A minimal Art-Net to DMX gateway setup is simple: any ESP board plus an RS485 transceiver.

The ESP32S3 is overkill for a gateway, but it's a really nice and fun chip to work with.
Field tests showed good results with it.

I put XIAO ESP32S3 and an RS485 interface found on Amazon on a simple board:

<img src="/assets/Xiao-Repeater.jpg">

This board has:

- Button wired to GPIO2
- LED wired to GPIO1
- DMX output wired to GPIO3
- SSD1306 I2C OLED screen connector
- SPI connector (with voltage set jumper)
- one free GPIO (GPIO4) - connect an external relay or a servo

### Schematic

<img src="/assets/Xiao-Repeater_schematic.png">

### XIAO ESP32S3

<img src="/assets/XIAOESP32S3.png" width="600" >
