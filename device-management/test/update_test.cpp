#include <gtest/gtest.h>
#include <device_management.h>

TEST(InitTest, DoubleInit) {
    device_management_init();
    device_management_init();
    device_management_fini();
}

TEST(InitTest, DoubleFini) {
    device_management_fini();
    device_management_fini();
}