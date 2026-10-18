#include "AutoRelease.h"
#include <print>

int main(void)
{
    auto test = AutoRelease<int>(5);
    std::println("Test object value: {}", test.Get());
    return 0;
}