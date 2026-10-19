#include "AutoRelease.h"
#include <print>

struct TestStruct
{
    auto printHello() const -> void { std::println("Hello From Test Struct!"); }
};

auto returnPtr() -> int*
{
    int *p = new int(5);
    return p;
}


int main(void)
{
    auto test = AutoRelease<TestStruct>(new TestStruct());
    test->printHello();
    // std::println("Test object value: {}", test.Get());
    int* local = returnPtr();
    auto localStorage = AutoRelease<int>(local);
    std::println("{}", *localStorage);
    return 0;
}