#include "AutoRelease.h"
#include <print>

struct TestStruct
{
    auto printHello() const -> void { std::println("Hello From Test Struct!"); }
};


int main(void)
{
    auto test = AutoRelease<TestStruct>(new TestStruct());
    test->printHello();
    // std::println("Test object value: {}", test.Get());
    return 0;
}