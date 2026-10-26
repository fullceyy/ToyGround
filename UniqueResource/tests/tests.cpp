// Dependency-free tests for UniqueResource. Build: g++ -std=c++23 tests.cpp
#include <cstdio>
#include <functional>
#include <vector>

#include "FakeApi.h"

// ---- tiny harness: failures are recorded, execution continues ---------------
static int g_checks = 0, g_failures = 0;
#define CHECK(expr)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(expr)) {                                                           \
            ++g_failures;                                                        \
            std::printf("    FAIL line %d: %s\n", __LINE__, #expr);              \
        }                                                                        \
    } while (0)

struct TestCase { const char* name; void (*fn)(); };
static std::vector<TestCase>& Tests() { static std::vector<TestCase> t; return t; }
struct Registrar { Registrar(const char* n, void (*f)()) { Tests().push_back({n, f}); } };
#define TEST(name)                                                               \
    static void name();                                                          \
    static Registrar reg_##name{#name, &name};                                   \
    static void name()

static void Fresh() { fake::Registry::Reset(); fake::Fds::Reset(); }

// ---- compile-time properties --------------------------------------------------
using UR = UniqueResource<unsigned>;
static_assert(!std::is_copy_constructible_v<UR>);
static_assert(!std::is_copy_assignable_v<UR>);
static_assert(std::is_nothrow_move_constructible_v<UR>);
static_assert(std::is_nothrow_move_assignable_v<UR>);
static_assert(sizeof(UniqueResource<unsigned, void (*)(unsigned)>) >= sizeof(unsigned));

// ---- construction & destruction ---------------------------------------------
TEST(default_constructed_is_empty)
{
    Fresh();
    {
        UR r;
        CHECK(!r);
        CHECK(r.Get() == 0u);
    }
    CHECK(fake::Registry::destroys == 0);       // deleter never called on sentinel
}

TEST(default_constructed_with_function_pointer_deleter_is_safe)
{
    Fresh();
    { UR r; (void)r; }                          // Del_ is uninitialised in the default ctor
    CHECK(fake::Registry::destroys == 0);
}

TEST(destructor_releases_exactly_once)
{
    Fresh();
    {
        UR r(0u, &fake::DestroyBuffer);
        fake::CreateBuffers(1, &r);
        CHECK(static_cast<bool>(r));
        CHECK(fake::Registry::live.size() == 1);
    }
    CHECK(fake::Registry::live.empty());
    CHECK(fake::Registry::destroys == 1);
    CHECK(fake::Registry::doubleFrees == 0);
}

TEST(sentinel_value_is_never_released)
{
    Fresh();
    {
        UR r(0u, &fake::DestroyBuffer);
        r.Reset(0u);
    }
    CHECK(fake::Registry::destroys == 0);
}

TEST(non_zero_sentinel_starts_empty)           // POSIX: -1 is empty, 0 is a real fd
{
    Fresh();
    {
        UniqueResource<int> r(-1, &fake::Close);
        CHECK(!r);                              // freshly constructed with sentinel -1
    }
    CHECK(fake::Fds::doubleCloses == 0);        // must not "close" fd 0 that was never opened
}

TEST(fd_zero_is_a_valid_resource)
{
    Fresh();
    {
        FileDesc f(fake::Open());               // fd 0
        CHECK(f.Valid());
        CHECK(f.Fd() == 0);
    }
    CHECK(fake::Fds::open.empty());
    CHECK(fake::Fds::doubleCloses == 0);
}

// ---- Reset ------------------------------------------------------------------
TEST(reset_releases_old_and_takes_new)
{
    Fresh();
    UR r(0u, &fake::DestroyBuffer);
    unsigned a = 0, b = 0;
    fake::CreateBuffers(1, &a);
    fake::CreateBuffers(1, &b);
    r.Reset(a);
    r.Reset(b);                                 // frees a
    CHECK(fake::Registry::live.count(a) == 0);
    CHECK(fake::Registry::live.count(b) == 1);
    CHECK(r.Get() == b);
    r.Reset(0u);                                // frees b
    CHECK(fake::Registry::live.empty());
    CHECK(fake::Registry::doubleFrees == 0);
}

// ---- moves --------------------------------------------------------------------
TEST(move_constructor_transfers_ownership)
{
    Fresh();
    {
        UR a(0u, &fake::DestroyBuffer);
        fake::CreateBuffers(1, &a);
        unsigned h = a.Get();
        UR b(std::move(a));
        CHECK(!a);                              // moved-from is empty
        CHECK(b.Get() == h);
        CHECK(fake::Registry::destroys == 0);
    }
    CHECK(fake::Registry::destroys == 1);
    CHECK(fake::Registry::doubleFrees == 0);
}

TEST(move_assignment_releases_old_and_empties_source)
{
    Fresh();
    {
        UR a(0u, &fake::DestroyBuffer);
        UR b(0u, &fake::DestroyBuffer);
        fake::CreateBuffers(1, &a);
        fake::CreateBuffers(1, &b);
        unsigned ha = a.Get(), hb = b.Get();
        a = std::move(b);
        CHECK(fake::Registry::live.count(ha) == 0);   // old resource of a freed
        CHECK(a.Get() == hb);
        CHECK(!b);                                    // source must be empty afterwards
    }
    CHECK(fake::Registry::live.empty());
    CHECK(fake::Registry::doubleFrees == 0);          // fails if source keeps its handle
}

TEST(move_assignment_from_empty_source)
{
    Fresh();
    {
        UR a(0u, &fake::DestroyBuffer);
        UR b(0u, &fake::DestroyBuffer);
        fake::CreateBuffers(1, &a);
        a = std::move(b);
        CHECK(!a);
        CHECK(fake::Registry::live.empty());
    }
    CHECK(fake::Registry::doubleFrees == 0);
}

TEST(self_move_assignment_keeps_resource)
{
    Fresh();
    {
        UR a(0u, &fake::DestroyBuffer);
        fake::CreateBuffers(1, &a);
        UR& alias = a;
        a = std::move(alias);
        CHECK(static_cast<bool>(a));
        CHECK(fake::Registry::destroys == 0);
    }
    CHECK(fake::Registry::doubleFrees == 0);
}

// ---- swap -----------------------------------------------------------------------
TEST(swap_exchanges_handles_and_deleters)
{
    Fresh();
    static int calledA = 0, calledB = 0;
    calledA = calledB = 0;
    {
        UR a(0u, [](unsigned h) { ++calledA; fake::DestroyBuffer(h); });
        UR b(0u, [](unsigned h) { ++calledB; fake::DestroyBuffer(h); });
        fake::CreateBuffers(1, &a);
        fake::CreateBuffers(1, &b);
        unsigned ha = a.Get(), hb = b.Get();
        a.Swap(b);
        CHECK(a.Get() == hb);
        CHECK(b.Get() == ha);
    }
    CHECK(calledA == 1 && calledB == 1);        // each deleter ran once
    CHECK(fake::Registry::doubleFrees == 0);
}

TEST(swap_carries_the_sentinel)
{
    Fresh();
    {
        UniqueResource<int> fd(-1, &fake::Close);
        UniqueResource<int> empty;              // sentinel 0, handle 0
        fd.Swap(empty);
        CHECK(!empty);
    }
    CHECK(fake::Fds::doubleCloses == 0);
}

// ---- conversions --------------------------------------------------------------
TEST(implicit_conversion_to_handle)
{
    Fresh();
    UR r(0u, &fake::DestroyBuffer);
    fake::CreateBuffers(1, &r);
    unsigned h = r;                             // operator T
    CHECK(h == r.Get());
    CHECK(r ? true : false);                    // operator bool
}

TEST(operator_address_of_is_pointer_to_handle)
{
    UR r(0u, &fake::DestroyBuffer);
    unsigned* p = &r;
    CHECK(p == std::addressof(*p));
    *p = 0u;                                    // leave empty so nothing is released
    CHECK(!r);
}

// ---- Buffer class ---------------------------------------------------------------
TEST(buffer_lifetime)
{
    Fresh();
    {
        Buffer b(64);
        CHECK(b.Valid());
        CHECK(b.Size() == 64);
        CHECK(fake::Registry::live.size() == 1);
    }
    CHECK(fake::Registry::live.empty());
    CHECK(fake::Registry::doubleFrees == 0);
}

TEST(buffer_default_is_empty)
{
    Fresh();
    { Buffer b; CHECK(!b.Valid()); }
    CHECK(fake::Registry::destroys == 0);
}

TEST(buffer_data_roundtrip)
{
    Fresh();
    Buffer b(4);
    b.Write(2, std::byte{0x7F});
    CHECK(b.Read(2) == std::byte{0x7F});
}

TEST(buffer_moves_are_rule_of_zero)
{
    Fresh();
    {
        Buffer a(8);
        unsigned h = a.Handle();
        Buffer b = std::move(a);
        CHECK(!a.Valid());
        CHECK(b.Handle() == h);
        Buffer c(16);
        c = std::move(b);                       // c's old buffer freed
        CHECK(fake::Registry::live.size() == 1);
        CHECK(c.Handle() == h);
    }
    CHECK(fake::Registry::live.empty());
    CHECK(fake::Registry::doubleFrees == 0);
}

TEST(buffer_in_vector_survives_reallocation)
{
    Fresh();
    {
        std::vector<Buffer> v;
        for (int i = 0; i < 100; ++i)
            v.emplace_back(static_cast<std::size_t>(i + 1));
        CHECK(fake::Registry::live.size() == 100);
        CHECK(fake::Registry::doubleFrees == 0);
        v.erase(v.begin(), v.begin() + 50);     // shifts via move assignment
        CHECK(fake::Registry::live.size() == 50);
    }
    CHECK(fake::Registry::live.empty());
    CHECK(fake::Registry::doubleFrees == 0);
}

TEST(buffer_early_release)
{
    Fresh();
    Buffer b(8);
    b.Release();
    CHECK(!b.Valid());
    CHECK(fake::Registry::live.empty());
}

// ---- runner -----------------------------------------------------------------------
int main()
{
    int failedTests = 0;
    for (const TestCase& t : Tests())
    {
        int before = g_failures;
        std::printf("[ RUN  ] %s\n", t.name);
        t.fn();
        bool ok = (g_failures == before);
        failedTests += !ok;
        std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", t.name);
    }
    std::printf("\n%zu tests, %d checks, %d failed tests, %d failed checks\n",
                Tests().size(), g_checks, failedTests, g_failures);
    return g_failures == 0 ? 0 : 1;
}