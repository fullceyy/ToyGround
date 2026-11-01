#pragma once

// DEFER schedules a function call to run when the scope ends.

// DEFER calls run in LIFO(stack) order, last deferred func call is the first to execute.

// Arguments are evaluated immediately, meaning that changes of the argument after deferred
// function declaration won't capture the modification. Exception cases can be seen in GO.

// DEFER can modify named return values, because they run after the return value is set
// but before the caller gets it.

// DEFER cleans up amount of branched call you'd have to write otherwise.

// Undo unless we succeed logic.

// move constructible if 

/*
The preprocessor’s rule:
when a macro parameter appears next to # or ##,
the argument is used as written,
without being expanded first.
Everywhere else, arguments are fully expanded before substitution.

without cc1(x, y) cc2(x, y) we'd get  DeferImpl cc2(defer_, __COUNTER__), which will not expand
and result in the name of the instantiated object to jus be defer___COUNTER__; not defer_0, 1, 2, 3;

The __COUNTER__ macro is a non-standard yet widely supported preprocessor macro in C and C++ 
(offered by MSVC, GCC, Clang, and others) that expands to an integer starting at 0 (or 1 in some older environments) 
and increments by 1 every time it is evaluated in a translation unit.
*/

#include <utility>
#include <exception>

#define cc2(x, y) x##y
#define cc1(x, y) cc2(x, y)

#define Defer(code) [[maybe_unused]] ToyGround::DeferImpl cc1(defer_, __COUNTER__){[&]{ code; }}
#define DeferFail(code) [[maybe_unused]] ToyGround::DeferImplScopeFail cc1(defer_, __COUNTER__){[&]{ code; }}
#define DeferSuccess(code) [[maybe_unused]] ToyGround::DeferImplScopeSuccess cc1(defer_, __COUNTER__){[&]{ code; }}

namespace ToyGround
{

template <typename T> // lambda type deduction?
class DeferImpl
{
private:
    T Call_;
    bool Flag_ {false};
public:
    explicit DeferImpl(T p) : Call_(std::move(p)) {}
    ~DeferImpl() { if(!Flag_) Call_(); }
    
    DeferImpl(const DeferImpl&) = delete;
    DeferImpl& operator=(const DeferImpl&) = delete;
    
    DeferImpl(DeferImpl&&) = delete;
    DeferImpl& operator=(DeferImpl&&) = delete;
    
    auto Dismiss() -> void { Flag_ = true; }
    // undo fn call passed as lambda -> risky fn call -> success -> .dismiss() don't call undo fn call
    // however can't call on macro, need actual var name.
};

template <typename T>
class DeferImplScopeFail
{
private:
    T Call_;
    bool Flag_ {false};
    int Exceptions_ {std::uncaught_exceptions()};
public:
    explicit DeferImplScopeFail(T p) : Call_(std::move(p)) {}
    ~DeferImplScopeFail() { if(!Flag_ && (std::uncaught_exceptions() > Exceptions_)) Call_(); }
    
    DeferImplScopeFail(const DeferImplScopeFail&) = delete;
    DeferImplScopeFail& operator=(const DeferImplScopeFail&) = delete;
    
    DeferImplScopeFail(DeferImplScopeFail&&) = delete;
    DeferImplScopeFail& operator=(DeferImplScopeFail&&) = delete;
    
    auto Dismiss() -> void { Flag_ = true; }
};

template <typename T>
class DeferImplScopeSuccess
{
private:
    T Call_;
    bool Flag_ {false};
    int Exceptions_ {std::uncaught_exceptions()};
public:
    explicit DeferImplScopeSuccess(T p) : Call_(std::move(p)) {}
    ~DeferImplScopeSuccess() { if(!Flag_ && (std::uncaught_exceptions() <= Exceptions_)) Call_(); }
    
    DeferImplScopeSuccess(const DeferImplScopeSuccess&) = delete;
    DeferImplScopeSuccess& operator=(const DeferImplScopeSuccess&) = delete;
    
    DeferImplScopeSuccess(DeferImplScopeSuccess&&) = delete;
    DeferImplScopeSuccess& operator=(DeferImplScopeSuccess&&) = delete;
    
    auto Dismiss() -> void { Flag_ = true; }
};

}