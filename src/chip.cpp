/* #include "chip.h"

String byteToHexString(uint8_t* buf, uint8_t length, String strSeperator) {
  String dataString = "";
  for (uint8_t i = 0; i < length; i++) {
    byte v = buf[i] / 16;
    byte w = buf[i] % 16;
    if (i>0) {
      dataString += strSeperator;
    }
    dataString += String(HEX_CHAR_ARRAY[v]);
    dataString += String(HEX_CHAR_ARRAY[w]);
  }
  dataString.toUpperCase();
  return dataString;
} // byteToHexString

#if !defined(ESP8266)
String getESP32ChipID() {
  uint64_t chipid;
  chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
  int chipid_size = 6;
  uint8_t chipid_arr[chipid_size];
  for (uint8_t i=0; i < chipid_size; i++) {
    chipid_arr[i] = (chipid >> (8 * i)) & 0xff;
  }
  return byteToHexString(chipid_arr, chipid_size, "");
}
#endif

String getChipID() {
  #if defined(ESP8266)
  return String(ESP.getChipId());
  #else
  return getESP32ChipID();
  #endif
} */