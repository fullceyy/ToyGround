// tests.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN   // doctest writes main() for you
#include "../doctest/doctest.h"
#include "AutoRelease.h"

TEST_CASE("move leaves the source empty")
{
    auto p = MakeAutoRelease<int>(5);
    auto q = std::move(p);
    CHECK(p == nullptr);
    CHECK(*q == 5);
}

TEST_CASE("dereferencing null throws")
{
    AutoRelease<int> p;
    CHECK_THROWS_AS(*p, NullAccessError);
}