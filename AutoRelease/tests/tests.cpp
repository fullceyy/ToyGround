// Dependency-free tests for AutoRelease<T, D> (the pointer-owning class).
// Build: g++ -std=c++23 -fsanitize=address,undefined AutoReleaseTests.cpp
// A failing test means a promise of the class is broken: fix the class, not the test.
#include <algorithm>
#include <concepts>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "AutoRelease.h"

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

#define CHECK_THROWS_AS(expr, ExType)                                            \
    do {                                                                         \
        ++g_checks;                                                              \
        try {                                                                    \
            (void)(expr);                                                        \
            ++g_failures;                                                        \
            std::printf("    FAIL line %d: %s did not throw\n", __LINE__, #expr);\
        } catch (const ExType&) {                                                \
        } catch (...) {                                                          \
            ++g_failures;                                                        \
            std::printf("    FAIL line %d: %s threw the wrong type\n", __LINE__, #expr); \
        }                                                                        \
    } while (0)

struct TestCase { const char* name; void (*fn)(); };
static std::vector<TestCase>& Tests() { static std::vector<TestCase> t; return t; }
struct Registrar { Registrar(const char* n, void (*f)()) { Tests().push_back({n, f}); } };
#define TEST(name)                                                               \
    static void name();                                                          \
    static Registrar reg_##name{#name, &name};                                   \
    static void name()

// ---- test types -----------------------------------------------------------------
// Counts live instances: alive == 0 at the end of a test proves no leak and no double destroy.
struct Tracked
{
    static inline int alive = 0;
    int value;
    Tracked(int v = 0) : value(v) { ++alive; }
    Tracked(const Tracked& o) : value(o.value) { ++alive; }
    ~Tracked() { --alive; }
};

// Polymorphic pair: deleting a Derived through AutoRelease<Base> must run both destructors.
struct Base    { static inline int dtors = 0; virtual ~Base() { ++dtors; } };
struct Derived : Base { static inline int dtors = 0; ~Derived() override { ++dtors; } };

// A constructor that throws after a member was built: nothing may leak.
struct Thrower
{
    Tracked member;
    Thrower() { throw std::runtime_error("ctor failed"); }
};

// Stateful deleter: records who released which pointer.
struct CountingDeleter
{
    struct Call { int id; const Tracked* ptr; };
    static inline std::vector<Call> log;
    int id = 0;
    void operator()(Tracked* p) const { log.push_back({id, p}); delete p; }
};
static int TimesCalled(int id, const Tracked* p)
{
    return static_cast<int>(std::count_if(CountingDeleter::log.begin(), CountingDeleter::log.end(),
        [&](const CountingDeleter::Call& c) { return c.id == id && c.ptr == p; }));
}

// Function-pointer deleter.
static int g_fnCalls = 0;
static void FreeTracked(Tracked* p) { ++g_fnCalls; delete p; }

// Empty deleter (for the sizeof check).
struct EmptyDel { void operator()(int* p) const { delete p; } };

static void Fresh()
{
    Tracked::alive = 0;
    Base::dtors = 0;
    Derived::dtors = 0;
    CountingDeleter::log.clear();
    g_fnCalls = 0;
}

// Concepts used to test "this must NOT compile" (requires-expressions must be dependent).
template <class X> concept CanDeref  = requires(X a) { *a; };
template <class X> concept CanArrow  = requires(X a) { a.operator->(); };
template <class X> concept CanIndex  = requires(X a) { a[0]; };
template <class X> concept CanLess   = requires(X a, X b) { a < b; };
template <class X> concept CanLessEq = requires(X a, X b) { a <= b; };
template <class X> concept CanGreater   = requires(X a, X b) { a > b; };
template <class X> concept CanGreaterEq = requires(X a, X b) { a >= b; };
template <class X> concept CanMakeArray = requires { MakeAutoRelease<X>(std::size_t{1}); };

// ---- compile-time properties ------------------------------------------------------
using AR = AutoRelease<Tracked>;
using ARArr = AutoRelease<Tracked[]>;
using ARFn = AutoRelease<Tracked, void (*)(Tracked*)>;

static_assert(!std::is_copy_constructible_v<AR>);
static_assert(!std::is_copy_assignable_v<AR>);
static_assert(std::is_nothrow_move_constructible_v<AR>);
static_assert(std::is_nothrow_move_assignable_v<AR>);
static_assert(noexcept(swap(std::declval<AR&>(), std::declval<AR&>())));
static_assert(noexcept(std::declval<AR&>().Release()));
static_assert(noexcept(std::declval<const AR&>().Get()));

// explicit constructors / conversions
static_assert(!std::is_convertible_v<Tracked*, AR>);        // no implicit ownership grab
static_assert(std::is_constructible_v<AR, Tracked*>);       // explicit is fine
static_assert(!std::is_convertible_v<AR, bool>);            // explicit operator bool
static_assert(std::is_constructible_v<bool, AR>);

// one-argument constructor is unavailable for a function-pointer deleter (it would be null)
static_assert(!std::is_constructible_v<ARFn, Tracked*>);
static_assert(std::is_constructible_v<ARFn, Tracked*, void (*)(Tracked*)>);

// size: an empty deleter costs nothing
static_assert(sizeof(AutoRelease<int>) == sizeof(int*));
static_assert(sizeof(AutoRelease<int, EmptyDel>) == sizeof(int*));
static_assert(sizeof(AutoRelease<int, void (*)(int*)>) > sizeof(int*));

// comparison: identity only
static_assert(std::equality_comparable<AR>);
static_assert(!std::totally_ordered<AR>);
static_assert(!CanLess<AR> && !CanLessEq<AR> && !CanGreater<AR> && !CanGreaterEq<AR>);

// array / non-array interface split
static_assert(CanDeref<AR> && CanArrow<AR> && !CanIndex<AR>);
static_assert(!CanDeref<ARArr> && !CanArrow<ARArr> && CanIndex<ARArr>);

// factory restrictions
static_assert(std::is_same_v<decltype(MakeAutoRelease<Tracked>(1)), AutoRelease<Tracked>>);
static_assert(CanMakeArray<Tracked[]>);
static_assert(!CanMakeArray<Tracked[3]>);    // bounded arrays rejected

// Current limitation, documented: no Derived -> Base converting move.
// Delete this assert if you add a converting constructor.
static_assert(!std::is_constructible_v<AutoRelease<Base>, AutoRelease<Derived>&&>);

// ---- construction & destruction -----------------------------------------------------
TEST(default_constructed_is_empty)
{
    Fresh();
    {
        AR p;
        CHECK(!p);
        CHECK(p.Get() == nullptr);
        CHECK(p == nullptr);
    }
    CHECK(Tracked::alive == 0);
}

TEST(destructor_releases_exactly_once)
{
    Fresh();
    {
        AR p(new Tracked(5));
        CHECK(Tracked::alive == 1);
        CHECK(p->value == 5);
    }
    CHECK(Tracked::alive == 0);
}

TEST(make_auto_release_forwards_arguments)
{
    Fresh();
    {
        auto p = MakeAutoRelease<Tracked>(42);
        CHECK(Tracked::alive == 1);
        CHECK((*p).value == 42);
        CHECK(p->value == 42);
    }
    CHECK(Tracked::alive == 0);
}

TEST(throwing_constructor_does_not_leak)
{
    Fresh();
    CHECK_THROWS_AS(MakeAutoRelease<Thrower>(), std::runtime_error);
    CHECK(Tracked::alive == 0);               // the already-built member was destroyed
}

TEST(polymorphic_delete_runs_both_destructors)
{
    Fresh();
    {
        AutoRelease<Base> p(new Derived);
        CHECK(Base::dtors == 0);
    }
    CHECK(Derived::dtors == 1);
    CHECK(Base::dtors == 1);
}

TEST(const_object_can_be_read)
{
    Fresh();
    const auto p = MakeAutoRelease<Tracked>(9);
    CHECK((*p).value == 9);
    CHECK(p->value == 9);
    CHECK(p.Get() != nullptr);
    CHECK(!(p == nullptr));
    CHECK(static_cast<bool>(p));
}

// ---- null access ---------------------------------------------------------------------
TEST(null_dereference_throws)
{
    Fresh();
    AR p;
    CHECK_THROWS_AS(*p, NullAccessError);
    CHECK_THROWS_AS(p.operator->(), NullAccessError);
}

TEST(null_error_is_a_logic_error_with_message)
{
    Fresh();
    AR p;
    try
    {
        (void)*p;
        CHECK(false);
    }
    catch (const std::logic_error& e)         // NullAccessError derives from logic_error
    {
        CHECK(std::string(e.what()).find("operator*") != std::string::npos);
    }
}

TEST(null_array_index_throws)
{
    Fresh();
    ARArr p;
    CHECK_THROWS_AS(p[0], NullAccessError);
}

TEST(access_after_release_throws)
{
    Fresh();
    auto p = MakeAutoRelease<Tracked>(1);
    Tracked* raw = p.Release();
    CHECK_THROWS_AS(*p, NullAccessError);
    delete raw;
}

// ---- Release / Reset --------------------------------------------------------------------
TEST(release_gives_up_ownership_without_freeing)
{
    Fresh();
    auto p = MakeAutoRelease<Tracked>(3);
    Tracked* raw = p.Release();
    CHECK(!p);
    CHECK(p.Get() == nullptr);
    CHECK(Tracked::alive == 1);               // still alive, caller owns it now
    delete raw;
    CHECK(Tracked::alive == 0);               // and the wrapper's destructor did not delete again
}

TEST(reset_with_new_pointer_frees_old)
{
    Fresh();
    auto p = MakeAutoRelease<Tracked>(1);
    Tracked* second = new Tracked(2);
    p.Reset(second);
    CHECK(Tracked::alive == 1);
    CHECK(p.Get() == second);
    CHECK(p->value == 2);
}

TEST(reset_without_argument_empties)
{
    Fresh();
    auto p = MakeAutoRelease<Tracked>(1);
    p.Reset();
    CHECK(!p);
    CHECK(Tracked::alive == 0);
    p.Reset();                                // resetting an empty object is harmless
    CHECK(Tracked::alive == 0);
}

TEST(reset_on_empty_adopts_pointer)
{
    Fresh();
    AR p;
    p.Reset(new Tracked(7));
    CHECK(static_cast<bool>(p));
    CHECK(p->value == 7);
}

// ---- moves ------------------------------------------------------------------------------
TEST(move_constructor_transfers_ownership)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked>(8);
        Tracked* raw = a.Get();
        AR b(std::move(a));
        CHECK(!a);                            // moved-from is empty
        CHECK(b.Get() == raw);
        CHECK(Tracked::alive == 1);
    }
    CHECK(Tracked::alive == 0);               // freed once
}

TEST(move_assignment_frees_old_and_empties_source)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked>(1);
        auto b = MakeAutoRelease<Tracked>(2);
        Tracked* rawB = b.Get();
        a = std::move(b);
        CHECK(Tracked::alive == 1);           // a's old object freed immediately
        CHECK(a.Get() == rawB);
        CHECK(!b);
    }
    CHECK(Tracked::alive == 0);
}

TEST(move_assignment_from_empty_source_empties_target)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked>(1);
        AR b;
        a = std::move(b);
        CHECK(!a);
        CHECK(Tracked::alive == 0);
    }
    CHECK(Tracked::alive == 0);
}

TEST(move_assignment_into_empty_target)
{
    Fresh();
    {
        AR a;
        auto b = MakeAutoRelease<Tracked>(4);
        a = std::move(b);
        CHECK(a->value == 4);
        CHECK(!b);
    }
    CHECK(Tracked::alive == 0);
}

TEST(self_move_assignment_keeps_resource)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked>(6);
        AR& alias = a;
        a = std::move(alias);
        CHECK(static_cast<bool>(a));
        CHECK(a->value == 6);
        CHECK(Tracked::alive == 1);
    }
    CHECK(Tracked::alive == 0);
}

TEST(returned_from_function_and_passed_to_sink)
{
    Fresh();
    auto factory = [] { return MakeAutoRelease<Tracked>(11); };
    auto sink = [](AR p) { return p->value; };
    {
        AR p = factory();
        CHECK(Tracked::alive == 1);
        CHECK(sink(std::move(p)) == 11);      // sink's parameter owned it and freed it
        CHECK(!p);
        CHECK(Tracked::alive == 0);
    }
}

// ---- swap -------------------------------------------------------------------------------
TEST(swap_exchanges_pointers)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked>(1);
        auto b = MakeAutoRelease<Tracked>(2);
        Tracked* pa = a.Get();
        Tracked* pb = b.Get();
        swap(a, b);
        CHECK(a.Get() == pb);
        CHECK(b.Get() == pa);
        CHECK(Tracked::alive == 2);
    }
    CHECK(Tracked::alive == 0);
}

TEST(swap_with_empty)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked>(1);
        AR b;
        swap(a, b);
        CHECK(!a);
        CHECK(static_cast<bool>(b));
    }
    CHECK(Tracked::alive == 0);
}

TEST(swap_with_itself_is_safe)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked>(1);
        swap(a, a);
        CHECK(static_cast<bool>(a));
        CHECK(Tracked::alive == 1);
    }
    CHECK(Tracked::alive == 0);
}

// ---- comparison (identity only) ----------------------------------------------------------
TEST(equality_compares_identity_not_value)
{
    Fresh();
    auto a = MakeAutoRelease<Tracked>(5);
    auto b = MakeAutoRelease<Tracked>(5);     // same value, different object
    CHECK(!(a == b));
    CHECK(a != b);
    CHECK(a == a);
}

TEST(two_empty_objects_are_equal)
{
    AR a, b;
    CHECK(a == b);
    CHECK(!(a != b));
}

TEST(comparison_with_nullptr_both_directions)
{
    Fresh();
    AR empty;
    auto full = MakeAutoRelease<Tracked>(1);
    CHECK(empty == nullptr);
    CHECK(nullptr == empty);
    CHECK(full != nullptr);
    CHECK(nullptr != full);
    CHECK(!(full == nullptr));
}

TEST(operator_bool_in_conditions)
{
    Fresh();
    AR empty;
    auto full = MakeAutoRelease<Tracked>(1);
    CHECK(!empty);
    CHECK(full ? true : false);
    if (empty) CHECK(false);
    if (!full) CHECK(false);
}

// ---- deleters ---------------------------------------------------------------------------
TEST(function_pointer_deleter_is_used)
{
    Fresh();
    {
        ARFn p(new Tracked(1), &FreeTracked);
        CHECK(g_fnCalls == 0);
    }
    CHECK(g_fnCalls == 1);
    CHECK(Tracked::alive == 0);
}

TEST(function_pointer_deleter_default_constructed_is_safe)
{
    Fresh();
    { ARFn p; CHECK(!p); }                    // null pointer and null deleter: nothing is called
    CHECK(g_fnCalls == 0);
}

TEST(lambda_deleter_is_used)
{
    Fresh();
    static int lambdaCalls = 0;
    lambdaCalls = 0;
    auto del = [](Tracked* p) { ++lambdaCalls; delete p; };
    {
        AutoRelease<Tracked, decltype(del)> p(new Tracked(1), del);
        CHECK(lambdaCalls == 0);
        auto q = std::move(p);                // move construction works with a closure type
        CHECK(!p);
    }
    CHECK(lambdaCalls == 1);                  // exactly once, by the final owner
    CHECK(Tracked::alive == 0);
    // Note: move assignment and swap need an assignable deleter, so they do not
    // compile for closure types. That is a documented limitation.
}

TEST(stateful_deleter_runs_with_its_state)
{
    Fresh();
    Tracked* raw = new Tracked(1);
    {
        AutoRelease<Tracked, CountingDeleter> p(raw, CountingDeleter{7});
    }
    CHECK(TimesCalled(7, raw) == 1);
    CHECK(CountingDeleter::log.size() == 1);
}

TEST(deleter_never_called_for_null)
{
    Fresh();
    {
        AutoRelease<Tracked, CountingDeleter> empty;
        AutoRelease<Tracked, CountingDeleter> moved(new Tracked, CountingDeleter{1});
        AutoRelease<Tracked, CountingDeleter> target(std::move(moved));   // 'moved' is now empty
        Tracked* raw = target.Release();                                  // 'target' is now empty
        delete raw;
    }
    CHECK(CountingDeleter::log.empty());
}

TEST(deleter_travels_with_the_pointer_on_move_construction)
{
    Fresh();
    Tracked* raw = new Tracked(1);
    {
        AutoRelease<Tracked, CountingDeleter> a(raw, CountingDeleter{3});
        AutoRelease<Tracked, CountingDeleter> b(std::move(a));
    }
    CHECK(TimesCalled(3, raw) == 1);
    CHECK(CountingDeleter::log.size() == 1);
}

TEST(move_assignment_uses_old_deleter_for_old_pointer_and_new_for_new)
{
    Fresh();
    Tracked* rawA = new Tracked(1);
    Tracked* rawB = new Tracked(2);
    {
        AutoRelease<Tracked, CountingDeleter> a(rawA, CountingDeleter{1});
        AutoRelease<Tracked, CountingDeleter> b(rawB, CountingDeleter{2});
        a = std::move(b);
        CHECK(TimesCalled(1, rawA) == 1);     // a's old object released by a's old deleter, right away
        CHECK(CountingDeleter::log.size() == 1);
    }
    CHECK(TimesCalled(2, rawB) == 1);         // b's object released by b's deleter
    CHECK(CountingDeleter::log.size() == 2);
}

TEST(swap_exchanges_deleters_with_pointers)
{
    Fresh();
    Tracked* rawA = new Tracked(1);
    Tracked* rawB = new Tracked(2);
    {
        AutoRelease<Tracked, CountingDeleter> a(rawA, CountingDeleter{1});
        AutoRelease<Tracked, CountingDeleter> b(rawB, CountingDeleter{2});
        swap(a, b);
    }
    CHECK(TimesCalled(1, rawA) == 1);         // each pointer was still released by its own deleter
    CHECK(TimesCalled(2, rawB) == 1);
    CHECK(CountingDeleter::log.size() == 2);
}

TEST(reset_uses_the_stored_deleter)
{
    Fresh();
    Tracked* first = new Tracked(1);
    Tracked* second = new Tracked(2);
    {
        AutoRelease<Tracked, CountingDeleter> p(first, CountingDeleter{9});
        p.Reset(second);
        CHECK(TimesCalled(9, first) == 1);
    }
    CHECK(TimesCalled(9, second) == 1);
}

// ---- arrays -----------------------------------------------------------------------------
TEST(array_make_and_index)
{
    Fresh();
    {
        auto p = MakeAutoRelease<Tracked[]>(5);
        CHECK(Tracked::alive == 5);
        p[0].value = 10;
        p[4].value = 40;
        CHECK(p[0].value == 10);
        CHECK(p[4].value == 40);
        CHECK(p[2].value == 0);               // value-initialised
    }
    CHECK(Tracked::alive == 0);               // delete[] ran every destructor
}

TEST(array_from_raw_pointer)
{
    Fresh();
    AutoRelease<int[]> p(new int[3]{1, 2, 3});
    CHECK(p[0] == 1 && p[1] == 2 && p[2] == 3);
}

TEST(array_move_and_reset)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked[]>(3);
        ARArr b(std::move(a));
        CHECK(!a);
        CHECK(Tracked::alive == 3);
        b.Reset(new Tracked[2]);
        CHECK(Tracked::alive == 2);           // old 3 released with delete[]
    }
    CHECK(Tracked::alive == 0);
}

TEST(array_move_assignment_frees_old_array)
{
    Fresh();
    {
        auto a = MakeAutoRelease<Tracked[]>(4);
        auto b = MakeAutoRelease<Tracked[]>(2);
        a = std::move(b);
        CHECK(Tracked::alive == 2);
    }
    CHECK(Tracked::alive == 0);
}

// ---- containers --------------------------------------------------------------------------
TEST(vector_reallocation_moves_instead_of_copying)
{
    Fresh();
    {
        std::vector<AR> v;
        for (int i = 0; i < 100; ++i)
            v.push_back(MakeAutoRelease<Tracked>(i));
        CHECK(Tracked::alive == 100);
        CHECK(v[0]->value == 0);
        CHECK(v[99]->value == 99);
    }
    CHECK(Tracked::alive == 0);
}

TEST(vector_erase_shifts_with_move_assignment)
{
    Fresh();
    {
        std::vector<AR> v;
        for (int i = 0; i < 10; ++i)
            v.push_back(MakeAutoRelease<Tracked>(i));
        v.erase(v.begin(), v.begin() + 4);
        CHECK(v.size() == 6);
        CHECK(Tracked::alive == 6);
        CHECK(v.front()->value == 4);
    }
    CHECK(Tracked::alive == 0);
}

TEST(vector_sort_by_value_with_projection)
{
    Fresh();
    std::vector<AR> v;
    for (int x : {5, 1, 4, 2, 3})
        v.push_back(MakeAutoRelease<Tracked>(x));
    std::ranges::sort(v, {}, [](const AR& p) -> int { return p->value; });
    for (int i = 0; i < 5; ++i)
        CHECK(v[static_cast<std::size_t>(i)]->value == i + 1);
    CHECK(Tracked::alive == 5);
}

// ---- runner -----------------------------------------------------------------------------
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