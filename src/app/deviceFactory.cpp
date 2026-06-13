#include "deviceFactory.h"

#include "boards/features.h"
#include "device/relay.h"
#if FEATURE_SERVO
#include "device/dmxServo.h"
#endif
#if FEATURE_DIMMER
#include "device/dimmer.h"
#endif
#if FEATURE_DMX_PORT
#include "device/repeater.h"
#endif

namespace app
{

std::unique_ptr<Device> makeDevice(const art::DeviceConfig &cfg, uint8_t universe)
{
    switch (cfg.type)
    {
    case art::DmxType::Relay:
        return std::make_unique<DmxRelay>(universe, cfg.channel, cfg.pin, cfg.level, cfg.threshold);

#if FEATURE_SERVO
    case art::DmxType::Servo:
        return std::make_unique<DmxServo>(universe, cfg.channel, cfg.pin);
#endif

#if FEATURE_DIMMER
    case art::DmxType::Dimmer:
        return std::make_unique<PwmDimmer>(universe, cfg.channel, cfg.pin, cfg.pulse, cfg.multiplier, cfg.level);
#endif

#if FEATURE_DMX_PORT
    case art::DmxType::Repeater:
        return std::make_unique<DmxRepeater>(universe);
#endif

    default:
        return nullptr;
    }
}

} // namespace app
