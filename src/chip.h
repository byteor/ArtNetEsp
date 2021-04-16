/* #if !defined(CHIP_H)
#define CHIP_H

#include <Arduino.h>

static const char HEX_CHAR_ARRAY[17] = "0123456789ABCDEF";
String byteToHexString(uint8_t* buf, uint8_t length, String strSeperator="-");

#if !defined(ESP8266)
String getESP32ChipID();
#endif

String getChipID();

#endif */