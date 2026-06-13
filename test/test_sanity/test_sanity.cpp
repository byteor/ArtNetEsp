#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_arithmetic(void)
{
    TEST_ASSERT_EQUAL(2, 1 + 1);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_arithmetic);
    return UNITY_END();
}
