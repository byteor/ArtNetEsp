#include <unity.h>
#include "core/dmxTypes.h"

using core::DmxType;
using core::fromWireString;
using core::toWireString;

void setUp(void) {}
void tearDown(void) {}

void test_toWireString_relay_is_legacy_binary(void)
{
    // R5 compat: Relay's wire string is "BINARY", never "RELAY".
    TEST_ASSERT_EQUAL_STRING("BINARY", toWireString(DmxType::Relay));
}

void test_toWireString_round_trip(void)
{
    TEST_ASSERT_EQUAL_STRING("DISABLED", toWireString(DmxType::Disabled));
    TEST_ASSERT_EQUAL_STRING("DIMMER", toWireString(DmxType::Dimmer));
    TEST_ASSERT_EQUAL_STRING("SERVO", toWireString(DmxType::Servo));
    TEST_ASSERT_EQUAL_STRING("REPEATER", toWireString(DmxType::Repeater));
}

void test_fromWireString_accepts_legacy_binary(void)
{
    TEST_ASSERT_EQUAL(DmxType::Relay, fromWireString("BINARY"));
}

void test_fromWireString_accepts_canonical_relay_alias(void)
{
    TEST_ASSERT_EQUAL(DmxType::Relay, fromWireString("RELAY"));
}

void test_fromWireString_round_trip(void)
{
    TEST_ASSERT_EQUAL(DmxType::Dimmer, fromWireString("DIMMER"));
    TEST_ASSERT_EQUAL(DmxType::Servo, fromWireString("SERVO"));
    TEST_ASSERT_EQUAL(DmxType::Repeater, fromWireString("REPEATER"));
}

void test_fromWireString_unknown_is_disabled(void)
{
    TEST_ASSERT_EQUAL(DmxType::Disabled, fromWireString("NOT_A_TYPE"));
    TEST_ASSERT_EQUAL(DmxType::Disabled, fromWireString(""));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_toWireString_relay_is_legacy_binary);
    RUN_TEST(test_toWireString_round_trip);
    RUN_TEST(test_fromWireString_accepts_legacy_binary);
    RUN_TEST(test_fromWireString_accepts_canonical_relay_alias);
    RUN_TEST(test_fromWireString_round_trip);
    RUN_TEST(test_fromWireString_unknown_is_disabled);
    return UNITY_END();
}
