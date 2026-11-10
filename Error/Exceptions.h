#include <stdexcept>
#include <stacktrace>
#include <format>
#include <print>
#include <source_location>

namespace ToyGround
{
/*
------------------------------------------------------------------------------------
Core idea

An error is only useful if you know where it started.

Once you’re inside a catch, the stack below it is already gone.

So the location has to be recorded when the error is created, not when it’s caught.
------------------------------------------------------------------------------------

------------------------------------------------------------------------------------
The three cases

Manual throw: your Error type records the location itself, at construction.

Standard-library throw that you catch: the original throw site is lost. 
You add your own location and a message on top.

Uncaught throw: a terminate handler can still see the original throw site, 
because libstdc++ doesn’t unwind the stack before calling it.
------------------------------------------------------------------------------------

------------------------------------------------------------------------------------
Capturing the location
std::source_location gives file, line, column and function name.

Put it as a default constructor argument, which fills it in at the throw line automatically.

No macro is needed, unlike Defer.

Don’t keep separate fields for line and function name, since source_location already holds them.
------------------------------------------------------------------------------------

------------------------------------------------------------------------------------
Capturing the stack trace

std::stacktrace::current() records the call chain.

Use it as a default argument too, so it starts at the throwing function.

Use current(), not current(1). Default arguments run in the caller’s frame, so current(1) skips the throwing function.

Store the raw trace and turn it into text only when printing. Capturing is cheap; resolving names is the expensive part.
------------------------------------------------------------------------------------

------------------------------------------------------------------------------------
Custom message and chaining

The message is the string passed to Error.

To wrap a standard exception, catch it and rethrow your Error with it nested inside (std::throw_with_nested).

This builds a chain, for example: “load level failed” → “bad config” → std::out_of_range.

A small walker function goes through the chain from outer to inner (std::rethrow_if_nested).
------------------------------------------------------------------------------------

*/


class CError : public std::runtime_error
{
private:
    std::source_location Location_;
    std::stacktrace Trace_;
    
    // std::string ConstructedErr_;
public:
    ~CError() = default;
    
    explicit CError(const std::string& pMsg, 
                    std::source_location pLoc = std::source_location::current(), 
                    std::stacktrace pTrace = std::stacktrace::current());
    
    auto Output() const -> void;
};


CError::CError(const std::string& pMsg, std::source_location pLoc, std::stacktrace pTrace)
: std::runtime_error{pMsg}, Location_(pLoc), Trace_(std::move(pTrace))
{
}

auto CError::Output() const -> void
{
    // ConstructedErr_ = std::format("{}\n{}, {}, {}:{}\nTrace:\n{}", 
    // std::runtime_error::what(),
    // Location_.file_name(),
    // Location_.function_name(),
    // Location_.line(),
    // Location_.column(),
    // to_string(Trace_));
    
    std::println(
    "{}\n{}, {}, {}:{}\nTrace:\n{}", 
    std::runtime_error::what(),
    Location_.file_name(),
    Location_.function_name(),
    Location_.line(),
    Location_.column(),
    to_string(Trace_));
}

// class Error : public std::runtime_error
// {
// private:
//     std::string err_;
// public:
//     variadic template constructor that takes forwarding references.
//     template <class... Args>
//     Error(std::format_string<Args...> msg, Args&&... args);
//     
//     auto what() const noexcept -> const char* override;
//     
//     auto Desc() const noexcept -> void;
// };

// template <class... Args>
// Error::Error(std::format_string<Args...> msg, Args&&... args)
// : std::runtime_error(std::format(msg, std::forward<Args>(args)...)),
// err_(std::format("STACKTRACE:\n{}WHAT:\n{}", std::stacktrace::current(1), std::runtime_error::what()))
// {}

// auto Error::what() const noexcept -> const char*
// {
//     return err_.c_str();
// }

}

