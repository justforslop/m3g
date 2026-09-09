#include "testfw.h"

int main(void)
{
    TESTFW_INIT();

    TESTFW_TEST_BEGIN("smoke: true is true");
    TESTFW_EXPECTED(1 == 1);
    TESTFW_TEST_END();

    return TESTFW_SUMMARY();
}
