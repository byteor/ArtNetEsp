#include <unity.h>

#include "core/dimmerLogic.h"

using core::DimmerLogic;

// HIGH/LOW as used by Arduino - not available in the native env.
constexpr uint8_t HIGH_STATE = 1;
constexpr uint8_t LOW_STATE = 0;

void setUp(void) {}
void tearDown(void) {}

// Steady dimming (no strobe): a DMX value maps directly to a constant duty,
// unaffected by tick().
void test_steady_dimming_constant_duty(void)
{
    DimmerLogic logic(HIGH_STATE);
    logic.setValue(150);

    TEST_ASSERT_EQUAL_UINT8(150, logic.duty());
    TEST_ASSERT_FALSE(logic.tick(0));
    TEST_ASSERT_EQUAL_UINT8(150, logic.duty());
    TEST_ASSERT_FALSE(logic.tick(1000));
    TEST_ASSERT_EQUAL_UINT8(150, logic.duty());
}

// Same as above but for an active-low device (e.g. a relay-style inverted
// output): the adjusted duty is 255 - value.
void test_steady_dimming_active_low(void)
{
    DimmerLogic logic(LOW_STATE);
    logic.setValue(100);

    TEST_ASSERT_EQUAL_UINT8(255 - 100, logic.duty());
}

// Strobe cycle: setDuration() (pulse) + setInterval() (period) make
// tick() alternate the duty between the DMX value and "off" on the
// pulse/period schedule.
void test_strobe_cycle_alternates(void)
{
    DimmerLogic logic(HIGH_STATE);
    logic.setValue(200);
    logic.setDuration(5);  // 'active' duration, ms
    logic.setInterval(50); // 'total' duration, ms (period > pulse -> strobing)

    // First tick flips active->inactive immediately (interval starts at 0).
    TEST_ASSERT_TRUE(logic.tick(0));
    TEST_ASSERT_EQUAL_UINT8(0, logic.duty());

    // Stays inactive until the remaining 45ms (period - pulse) elapse.
    TEST_ASSERT_FALSE(logic.tick(44));
    TEST_ASSERT_EQUAL_UINT8(0, logic.duty());

    // At +45ms, flips back to active for `pulse` (5ms).
    TEST_ASSERT_TRUE(logic.tick(45));
    TEST_ASSERT_EQUAL_UINT8(200, logic.duty());

    TEST_ASSERT_FALSE(logic.tick(49));
    TEST_ASSERT_EQUAL_UINT8(200, logic.duty());

    // At +5ms more (50ms total), flips inactive again.
    TEST_ASSERT_TRUE(logic.tick(50));
    TEST_ASSERT_EQUAL_UINT8(0, logic.duty());
}

// B18: flipping ON while no DMX value has ever been received (value == 0)
// brings the light to full, not to the (zero) adjusted DMX value.
void test_flip_to_full_when_value_zero(void)
{
    DimmerLogic logic(HIGH_STATE);

    // Flip OFF first (logic starts enabled).
    logic.flip();
    TEST_ASSERT_FALSE(logic.isEnabled());
    TEST_ASSERT_EQUAL_UINT8(0, logic.duty());

    // Flip back ON with value still 0 -> full brightness, not 0.
    logic.flip();
    TEST_ASSERT_TRUE(logic.isEnabled());
    TEST_ASSERT_EQUAL_UINT8(255, logic.duty());
}

// Flipping ON after a real DMX value has been received restores that value
// (not full brightness) - B18 only applies to the value==0 case.
void test_flip_restores_dmx_value_when_nonzero(void)
{
    DimmerLogic logic(HIGH_STATE);
    logic.setValue(80);

    logic.flip(); // OFF
    TEST_ASSERT_EQUAL_UINT8(0, logic.duty());

    logic.flip(); // ON
    TEST_ASSERT_EQUAL_UINT8(80, logic.duty());
}

// B17 fix (Phase 5 item 8): PwmDimmer::onDmx now re-applies setDuration()/
// setInterval() from the strobe-speed channel on EVERY incoming frame
// (~44fps), not just on change. Pin that repeating the same
// setDuration()/setInterval() values mid-cycle doesn't reset/disrupt an
// in-progress strobe cycle (no interval/state/previousMillis side effects).
void test_repeated_setduration_setinterval_does_not_disrupt_cycle(void)
{
    DimmerLogic logic(HIGH_STATE);
    logic.setValue(200);
    logic.setDuration(5);
    logic.setInterval(50);

    TEST_ASSERT_TRUE(logic.tick(0));
    TEST_ASSERT_EQUAL_UINT8(0, logic.duty());

    // Simulate per-frame onDmx re-applying the same strobe-speed channel
    // while inactive, partway through the 45ms "off" segment.
    logic.setDuration(5);
    logic.setInterval(50);
    TEST_ASSERT_FALSE(logic.tick(44));
    TEST_ASSERT_EQUAL_UINT8(0, logic.duty());

    // Cycle still completes on schedule (+45ms -> active again).
    logic.setDuration(5);
    logic.setInterval(50);
    TEST_ASSERT_TRUE(logic.tick(45));
    TEST_ASSERT_EQUAL_UINT8(200, logic.duty());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_steady_dimming_constant_duty);
    RUN_TEST(test_steady_dimming_active_low);
    RUN_TEST(test_strobe_cycle_alternates);
    RUN_TEST(test_flip_to_full_when_value_zero);
    RUN_TEST(test_flip_restores_dmx_value_when_nonzero);
    RUN_TEST(test_repeated_setduration_setinterval_does_not_disrupt_cycle);
    return UNITY_END();
}
