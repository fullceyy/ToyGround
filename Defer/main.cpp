#include "Defer.h"

#include <cstdio>
#include <stdexcept>
#include <string>

// ---- tiny test harness ------------------------------------------------------
static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (cond) { ++g_passed; }                                          \
        else { ++g_failed;                                                 \
               std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

#define TEST(name) static void name()
#define RUN(name) do { std::printf("%s\n", #name); name(); } while (0)

// ---- Defer -----------------------------------------------------------------

TEST(defer_runs_at_scope_exit_not_before)
{
    int x = 0;
    {
        Defer(x = 1);
        CHECK(x == 0);          // not run yet
    }
    CHECK(x == 1);              // ran at the closing brace
}

TEST(defer_runs_in_reverse_order)
{
    std::string order;
    {
        Defer(order += 'a');
        Defer(order += 'b');
        Defer(order += 'c');
    }
    CHECK(order == "cba");
}

static void early_return_helper(int& x, bool bail)
{
    Defer(x += 1);
    if (bail) return;
    x += 10;
}

TEST(defer_runs_on_early_return)
{
    int x = 0;
    early_return_helper(x, true);
    CHECK(x == 1);
    early_return_helper(x, false);
    CHECK(x == 12);
}

TEST(defer_runs_on_exception)
{
    int x = 0;
    try {
        Defer(x = 1);
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
        CHECK(x == 1);          // guard ran during unwinding, before the catch
    }
    CHECK(x == 1);
}

TEST(defer_sees_value_at_exit_not_at_creation)
{
    int seen = -1;
    {
        int v = 1;
        Defer(seen = v);        // [&] capture: reads v when the guard fires
        v = 2;
    }
    CHECK(seen == 2);
}

TEST(defer_runs_every_loop_iteration)
{
    int count = 0;
    for (int i = 0; i < 3; ++i) {
        Defer(++count);
        CHECK(count == i);      // previous iterations' guards already ran
    }
    CHECK(count == 3);
}

TEST(defer_dismiss_with_named_variable)
{
    int undo = 0;
    {
        ToyGround::DeferImpl guard{[&] { ++undo; }};
        guard.Dismiss();
    }
    CHECK(undo == 0);
}

TEST(defer_dismiss_rollback_pattern)
{
    int undo = 0;
    auto risky = [](bool fail) { if (fail) throw std::runtime_error("fail"); };

    // success path: dismiss reached, undo skipped
    {
        ToyGround::DeferImpl rollback{[&] { ++undo; }};
        risky(false);
        rollback.Dismiss();
    }
    CHECK(undo == 0);

    // failure path: throw skips Dismiss, undo runs
    try {
        ToyGround::DeferImpl rollback{[&] { ++undo; }};
        risky(true);
        rollback.Dismiss();
    } catch (...) {}
    CHECK(undo == 1);
}

static int add(int a, int b) { return a + b; }

TEST(defer_comma_inside_parentheses_is_fine)
{
    int x = 0;
    {
        Defer(x = add(2, 3));   // comma is protected by the parentheses
    }
    CHECK(x == 5);
}

// ---- DeferFail -------------------------------------------------------------

TEST(fail_does_not_run_on_normal_exit)
{
    int x = 0;
    {
        DeferFail(x = 1);
    }
    CHECK(x == 0);
}

TEST(fail_runs_on_exception)
{
    int x = 0;
    try {
        DeferFail(x = 1);
        throw std::runtime_error("boom");
    } catch (...) {}
    CHECK(x == 1);
}

TEST(fail_dismiss_prevents_run)
{
    int x = 0;
    try {
        ToyGround::DeferImplScopeFail guard{[&] { x = 1; }};
        guard.Dismiss();
        throw std::runtime_error("boom");
    } catch (...) {}
    CHECK(x == 0);
}

// ---- DeferSuccess ----------------------------------------------------------

TEST(success_runs_on_normal_exit)
{
    int x = 0;
    {
        DeferSuccess(x = 1);
    }
    CHECK(x == 1);
}

TEST(success_does_not_run_on_exception)
{
    int x = 0;
    try {
        DeferSuccess(x = 1);
        throw std::runtime_error("boom");
    } catch (...) {}
    CHECK(x == 0);
}

TEST(success_dismiss_prevents_run)
{
    int x = 0;
    {
        ToyGround::DeferImplScopeSuccess guard{[&] { x = 1; }};
        guard.Dismiss();
    }
    CHECK(x == 0);
}

// ---- guards created while another exception is already unwinding ----------
// This is the case the uncaught_exceptions() *count* exists for.

struct UnwindProbe
{
    int& fail_ran;
    int& success_ran;
    ~UnwindProbe()
    {
        // We are inside a destructor running because of an exception,
        // but this inner scope itself finishes normally.
        DeferFail(++fail_ran);
        DeferSuccess(++success_ran);
    }
};

TEST(guards_inside_unwinding_destructor_judge_their_own_scope)
{
    int fail_ran = 0;
    int success_ran = 0;
    try {
        UnwindProbe probe{fail_ran, success_ran};
        throw std::runtime_error("outer");
    } catch (...) {}
    CHECK(fail_ran == 0);       // with `> 0` instead of `> Exceptions_`, this would be 1
    CHECK(success_ran == 1);
}

// ---- mixing all three ------------------------------------------------------

static void mixed(std::string& log, bool fail)
{
    Defer(log += "always;");
    DeferFail(log += "fail;");
    DeferSuccess(log += "success;");
    if (fail) throw std::runtime_error("x");
}

TEST(mixed_guards_normal_and_exception)
{
    std::string log;
    mixed(log, false);
    CHECK(log == "success;always;");   // reverse order, fail guard skipped

    log.clear();
    try { mixed(log, true); } catch (...) {}
    CHECK(log == "fail;always;");      // reverse order, success guard skipped
}

// ----------------------------------------------------------------------------

int main()
{
    RUN(defer_runs_at_scope_exit_not_before);
    RUN(defer_runs_in_reverse_order);
    RUN(defer_runs_on_early_return);
    RUN(defer_runs_on_exception);
    RUN(defer_sees_value_at_exit_not_at_creation);
    RUN(defer_runs_every_loop_iteration);
    RUN(defer_dismiss_with_named_variable);
    RUN(defer_dismiss_rollback_pattern);
    RUN(defer_comma_inside_parentheses_is_fine);

    RUN(fail_does_not_run_on_normal_exit);
    RUN(fail_runs_on_exception);
    RUN(fail_dismiss_prevents_run);

    RUN(success_runs_on_normal_exit);
    RUN(success_does_not_run_on_exception);
    RUN(success_dismiss_prevents_run);

    RUN(guards_inside_unwinding_destructor_judge_their_own_scope);
    RUN(mixed_guards_normal_and_exception);

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}