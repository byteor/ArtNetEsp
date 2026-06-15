#include <unity.h>

#include <ArduinoJson.h>

#include "core/configCodec.h"
#include "core/configModel.h"
#include "core/dmxTypes.h"

using core::DeviceConfig;
using core::DmxType;
using core::HardwareConfig;
using core::WifiNet;

// HIGH/LOW as used by Arduino - not available in the native env.
constexpr uint8_t HIGH_STATE = 1;
constexpr uint8_t LOW_STATE = 0;

void setUp(void) {}
void tearDown(void) {}

static DeviceConfig sampleDeviceConfig()
{
    DeviceConfig cfg;
    cfg.channel = 7;
    cfg.type = DmxType::Dimmer;
    cfg.threshold = 99;
    cfg.pulse = 250;
    cfg.multiplier = 3;
    cfg.pin = 14;
    cfg.level = HIGH_STATE;
    cfg.blackout = false;
    return cfg;
}

static void assertDeviceConfigEqual(const DeviceConfig &a, const DeviceConfig &b)
{
    TEST_ASSERT_EQUAL(a.channel, b.channel);
    TEST_ASSERT_EQUAL(static_cast<int>(a.type), static_cast<int>(b.type));
    TEST_ASSERT_EQUAL(a.threshold, b.threshold);
    TEST_ASSERT_EQUAL(a.pulse, b.pulse);
    TEST_ASSERT_EQUAL(a.multiplier, b.multiplier);
    TEST_ASSERT_EQUAL(a.pin, b.pin);
    TEST_ASSERT_EQUAL(a.level, b.level);
    TEST_ASSERT_EQUAL(a.blackout, b.blackout);
}

static DeviceConfig defaultDeviceConfig()
{
    DeviceConfig defaults;
    defaults.channel = 0;
    defaults.type = DmxType::Disabled;
    defaults.threshold = 127;
    defaults.pulse = 10;
    defaults.multiplier = 1;
    defaults.pin = 2;
    defaults.level = LOW_STATE;
    defaults.blackout = true;
    return defaults;
}

void test_dmxChannel_roundtrip_full_fields(void)
{
    DeviceConfig original = sampleDeviceConfig();

    JsonDocument doc;
    core::dmxChannelToJson(original, doc.to<JsonObject>());

    DeviceConfig parsed = core::dmxChannelFromJson(doc.as<JsonObject>(), defaultDeviceConfig());

    assertDeviceConfigEqual(original, parsed);
}

void test_dmxChannel_legacy_binary_type_string(void)
{
    // R5 compat: "BINARY" (and "RELAY") on the wire both map to DmxType::Relay.
    JsonDocument doc;
    doc["type"] = "BINARY";
    DeviceConfig fromBinary = core::dmxChannelFromJson(doc.as<JsonObject>(), defaultDeviceConfig());
    TEST_ASSERT_EQUAL(static_cast<int>(DmxType::Relay), static_cast<int>(fromBinary.type));

    doc["type"] = "RELAY";
    DeviceConfig fromRelay = core::dmxChannelFromJson(doc.as<JsonObject>(), defaultDeviceConfig());
    TEST_ASSERT_EQUAL(static_cast<int>(DmxType::Relay), static_cast<int>(fromRelay.type));

    // toJson always serializes Relay back out as "BINARY" (compat contract).
    JsonDocument out;
    core::dmxChannelToJson(fromRelay, out.to<JsonObject>());
    TEST_ASSERT_EQUAL_STRING("BINARY", out["type"].as<const char *>());
}

void test_dmxChannel_defaults_applied_when_missing(void)
{
    JsonDocument doc;
    JsonObject empty = doc.to<JsonObject>();

    DeviceConfig defaults = defaultDeviceConfig();
    DeviceConfig parsed = core::dmxChannelFromJson(empty, defaults);

    TEST_ASSERT_EQUAL(defaults.channel, parsed.channel);
    TEST_ASSERT_EQUAL(static_cast<int>(DmxType::Disabled), static_cast<int>(parsed.type));
    TEST_ASSERT_EQUAL(defaults.threshold, parsed.threshold);
    TEST_ASSERT_EQUAL(defaults.pulse, parsed.pulse);
    TEST_ASSERT_EQUAL(defaults.multiplier, parsed.multiplier);
    TEST_ASSERT_EQUAL(defaults.pin, parsed.pin);
    // Missing "level" never matches "high" -> LOW, regardless of defaults.level.
    TEST_ASSERT_EQUAL(LOW_STATE, parsed.level);
    TEST_ASSERT_EQUAL(defaults.blackout, parsed.blackout);
}

void test_hardware_roundtrip(void)
{
    HardwareConfig original;
    original.pwmFreq = 5000;
    original.ledPin = 13;
    original.buttonPin = 4;
    original.longPressDelay = 3000;
    original.wifiPowerSave = true;
    original.authEnabled = true;
    original.authUser = "admin";
    original.authPass = "s3cr3t";

    JsonDocument doc;
    core::hardwareToJson(original, doc.to<JsonObject>());

    HardwareConfig defaults{};
    HardwareConfig parsed = core::hardwareFromJson(doc.as<JsonObject>(), defaults);

    TEST_ASSERT_EQUAL(original.pwmFreq, parsed.pwmFreq);
    TEST_ASSERT_EQUAL(original.ledPin, parsed.ledPin);
    TEST_ASSERT_EQUAL(original.buttonPin, parsed.buttonPin);
    TEST_ASSERT_EQUAL(original.longPressDelay, parsed.longPressDelay);
    TEST_ASSERT_EQUAL(original.wifiPowerSave, parsed.wifiPowerSave);
    TEST_ASSERT_EQUAL(original.authEnabled, parsed.authEnabled);
    TEST_ASSERT_EQUAL_STRING(original.authUser.c_str(), parsed.authUser.c_str());
    TEST_ASSERT_EQUAL_STRING(original.authPass.c_str(), parsed.authPass.c_str());
}

void test_hardware_auth_defaults_to_off(void)
{
    // R5 compat: an "hw" object that predates authEnabled/authUser/authPass
    // (no such keys at all) must parse with auth disabled.
    JsonDocument doc;
    doc["freq"] = 2000;
    doc["ledPin"] = 2;
    doc["buttonPin"] = 0;
    doc["longPressDelay"] = 5000;

    HardwareConfig parsed = core::hardwareFromJson(doc.as<JsonObject>(), HardwareConfig{});

    TEST_ASSERT_FALSE(parsed.authEnabled);
    TEST_ASSERT_EQUAL_STRING("", parsed.authUser.c_str());
    TEST_ASSERT_EQUAL_STRING("", parsed.authPass.c_str());
    // An "hw" object that predates wifiPowerSave must default to false
    // (radio always on / power save disabled).
    TEST_ASSERT_FALSE(parsed.wifiPowerSave);
}

void test_hardware_partial_update_keeps_defaults(void)
{
    JsonDocument doc;
    doc["freq"] = 2000;
    // ledPin/buttonPin/longPressDelay intentionally absent.

    HardwareConfig defaults;
    defaults.pwmFreq = 1000;
    defaults.ledPin = 2;
    defaults.buttonPin = 0;
    defaults.longPressDelay = 5000;

    HardwareConfig parsed = core::hardwareFromJson(doc.as<JsonObject>(), defaults);

    TEST_ASSERT_EQUAL(2000, parsed.pwmFreq);
    TEST_ASSERT_EQUAL(defaults.ledPin, parsed.ledPin);
    TEST_ASSERT_EQUAL(defaults.buttonPin, parsed.buttonPin);
    TEST_ASSERT_EQUAL(defaults.longPressDelay, parsed.longPressDelay);
}

void test_wifiNet_roundtrip(void)
{
    WifiNet original;
    original.ssid = "MyNetwork";
    original.pass = "s3cr3t";
    original.dhcp = true;
    original.order = 2;

    JsonDocument doc;
    core::wifiNetToJson(original, doc.to<JsonObject>());

    WifiNet parsed = core::wifiNetFromJson(doc.as<JsonObject>());

    TEST_ASSERT_EQUAL_STRING(original.ssid.c_str(), parsed.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(original.pass.c_str(), parsed.pass.c_str());
    TEST_ASSERT_EQUAL(original.dhcp, parsed.dhcp);
    TEST_ASSERT_EQUAL(original.order, parsed.order);
}

void test_wifiNet_dhcp_always_true_on_read(void)
{
    JsonDocument doc;
    doc["ssid"] = "Net";
    doc["pass"] = "pw";
    doc["order"] = 1;
    // No "dhcp" key present at all.

    WifiNet parsed = core::wifiNetFromJson(doc.as<JsonObject>());

    TEST_ASSERT_TRUE(parsed.dhcp);
}

// A full-Config-shaped JSON doc (wifi[] + dmx[] + hw), pushed through the
// codec functions for every entry, save -> load -> deep-equal.
void test_full_config_shaped_doc_roundtrip(void)
{
    std::vector<WifiNet> wifiOriginal = {
        {"Home", "homepass", true, 1},
        {"Backup", "backuppass", true, 2},
    };
    std::vector<DeviceConfig> dmxOriginal = {
        sampleDeviceConfig(),
        defaultDeviceConfig(),
    };
    HardwareConfig hwOriginal;
    hwOriginal.pwmFreq = 1500;
    hwOriginal.ledPin = 16;
    hwOriginal.buttonPin = 1;
    hwOriginal.longPressDelay = 4000;

    // Serialize, as configToJson would.
    JsonDocument doc;
    JsonArray wifiArr = doc["wifi"].to<JsonArray>();
    for (const auto &net : wifiOriginal)
        core::wifiNetToJson(net, wifiArr.add<JsonObject>());
    JsonArray dmxArr = doc["dmx"].to<JsonArray>();
    for (const auto &ch : dmxOriginal)
        core::dmxChannelToJson(ch, dmxArr.add<JsonObject>());
    core::hardwareToJson(hwOriginal, doc["hw"].to<JsonObject>());

    // Round-trip through a serialized string, as save()/load() would.
    std::string serialized;
    serializeJson(doc, serialized);
    JsonDocument loaded;
    DeserializationError err = deserializeJson(loaded, serialized);
    TEST_ASSERT_FALSE(err);

    // Deserialize, as configFromJson would.
    std::vector<WifiNet> wifiParsed;
    for (JsonObject net : loaded["wifi"].as<JsonArray>())
        wifiParsed.push_back(core::wifiNetFromJson(net));
    std::vector<DeviceConfig> dmxParsed;
    DeviceConfig defaults = defaultDeviceConfig();
    for (JsonObject ch : loaded["dmx"].as<JsonArray>())
        dmxParsed.push_back(core::dmxChannelFromJson(ch, defaults));
    HardwareConfig hwParsed = core::hardwareFromJson(loaded["hw"], HardwareConfig{});

    TEST_ASSERT_EQUAL(wifiOriginal.size(), wifiParsed.size());
    for (size_t i = 0; i < wifiOriginal.size(); i++)
    {
        TEST_ASSERT_EQUAL_STRING(wifiOriginal[i].ssid.c_str(), wifiParsed[i].ssid.c_str());
        TEST_ASSERT_EQUAL_STRING(wifiOriginal[i].pass.c_str(), wifiParsed[i].pass.c_str());
        TEST_ASSERT_EQUAL(wifiOriginal[i].dhcp, wifiParsed[i].dhcp);
        TEST_ASSERT_EQUAL(wifiOriginal[i].order, wifiParsed[i].order);
    }

    TEST_ASSERT_EQUAL(dmxOriginal.size(), dmxParsed.size());
    for (size_t i = 0; i < dmxOriginal.size(); i++)
        assertDeviceConfigEqual(dmxOriginal[i], dmxParsed[i]);

    TEST_ASSERT_EQUAL(hwOriginal.pwmFreq, hwParsed.pwmFreq);
    TEST_ASSERT_EQUAL(hwOriginal.ledPin, hwParsed.ledPin);
    TEST_ASSERT_EQUAL(hwOriginal.buttonPin, hwParsed.buttonPin);
    TEST_ASSERT_EQUAL(hwOriginal.longPressDelay, hwParsed.longPressDelay);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_dmxChannel_roundtrip_full_fields);
    RUN_TEST(test_dmxChannel_legacy_binary_type_string);
    RUN_TEST(test_dmxChannel_defaults_applied_when_missing);
    RUN_TEST(test_hardware_roundtrip);
    RUN_TEST(test_hardware_auth_defaults_to_off);
    RUN_TEST(test_hardware_partial_update_keeps_defaults);
    RUN_TEST(test_wifiNet_roundtrip);
    RUN_TEST(test_wifiNet_dhcp_always_true_on_read);
    RUN_TEST(test_full_config_shaped_doc_roundtrip);
    return UNITY_END();
}
