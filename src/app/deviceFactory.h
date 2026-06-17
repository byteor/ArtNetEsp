#ifndef APP_DEVICE_FACTORY_H
#define APP_DEVICE_FACTORY_H

#include <memory>
#include "config.h"
#include "device/device.h"

namespace app
{

// Constructs the Device for a single configured DMX channel, or nullptr for
// art::DmxType::Disabled / an unsupported type on this board (e.g. Servo on
// a FEATURE_SERVO=0 board). Device headers/feature guards live in
// deviceFactory.cpp - callers only need device.h.
std::unique_ptr<Device> makeDevice(const art::DeviceConfig &cfg, uint8_t universe);

} // namespace app

#endif // APP_DEVICE_FACTORY_H
