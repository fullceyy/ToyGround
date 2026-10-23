#include "AutoRelease.h"
#include <cstdio>

int main(void)
{
    auto a = MakeAutoRelease<int>(5);
    auto b = std::move(a);
    if (a == nullptr && b != nullptr) std::puts("move ok");

    auto c = MakeAutoRelease<int>(7);
    b = std::move(c);                 // old 5 freed, no leak
    if (*b == 7) std::puts("assign ok");

    auto arr = MakeAutoRelease<int[]>(3);
    arr[1] = 42;

    AutoRelease<std::FILE, decltype(&std::fclose)> f(std::fopen("x.txt", "w"), &std::fclose);

    static_assert(std::equality_comparable<AutoRelease<int>>);
    static_assert(!std::totally_ordered<AutoRelease<int>>);
    // AutoRelease<int> bad = new int(1);   // should not compile: explicit
    // a < b;                               // should not compile: deleted
    return 0;
}