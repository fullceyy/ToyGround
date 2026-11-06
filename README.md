# ToyGround

A playground for modern C++ (C++23) utilities, built to learn by reimplementing standard-library-style types. Every utility is header-only, sits in its own folder with its own tests, and records the reasoning behind its design choices.

| Utility | What it owns | Closest standard equivalent |
|---|---|---|
| [`AutoRelease`](AutoRelease/AutoRelease.h) | a heap object or array, through a pointer | `std::unique_ptr` |
| [`UniqueResource`](UniqueResource/UniqueResource.h) | a plain handle value (`GLuint`, file descriptor, `FILE*`) | `std::unique_resource` (similar) |
| [`Defer`](Defer/Defer.h) | a cleanup action to run at scope exit, on failure, or on success | `std::experimental::scope_exit` / `scope_fail` / `scope_success` (similar) |
| `OutPtr` | In Progress... | `std::out_ptr` |
| `Error` | In Progress... | In Progress... |

## Building

The project uses [xmake](https://xmake.io) and needs a C++23 compiler (GCC 14+ or Clang 18+).

```sh
xmake                              # configure and build
xmake test                         # build and run every test suite
xmake run [target_name_here]Tests  # run one test suite
xmake run [target_name_here]       # small example program
```

The root `xmake.lua` selects the MinGW toolchain (`set_toolchains("mingw")`). On Linux or macOS, remove that line or override it with `xmake f --toolchain=gcc` (or `clang`).

The tests depend only on the standard library, so they can also be built by hand:

```sh
g++ -std=c++23 -Wall -Wextra -fsanitize=address,undefined -IAutoRelease AutoRelease/tests/tests.cpp -o ar_tests
./ar_tests
```

## AutoRelease

A move-only owning smart pointer for single objects and unbounded arrays.

```cpp
#include "AutoRelease.h"

auto p = MakeAutoRelease<Widget>(42);      // like std::make_unique
p->Draw();

auto arr = MakeAutoRelease<int[]>(16);     // value-initialised array, released with delete[]
arr[3] = 7;

AutoRelease<std::FILE, decltype(&std::fclose)> file(std::fopen("log.txt", "w"), &std::fclose);
```

**Interface:** `Get`, `Release`, `Reset`, `Swap`, `explicit operator bool`, `*`, `->`, and `[]` (arrays only), plus `==` against another `AutoRelease` or against `nullptr`.

### Design decisions

- **Dereferencing null throws** `NullAccessError` (a `std::logic_error`) instead of being undefined behaviour. This is the main difference from `std::unique_ptr`. It costs one well-predicted branch per access.
- **Comparison is identity only.** `a == b` asks whether both own the *same object*. The relational operators are deleted, because ordering pointers by address is rarely meaningful, and ordering by value would contradict `==`. To sort by value, use a projection: `std::ranges::sort(v, {}, [](auto& p) -> auto& { return *p; })`.
- **Empty deleters cost nothing.** `[[no_unique_address]]` keeps `sizeof(AutoRelease<T>) == sizeof(T*)` (GCC and Clang; MSVC needs `[[msvc::no_unique_address]]`).
- **No silent ownership.** The pointer constructor is `explicit`, so `AutoRelease<int> p = raw;` does not compile.
- **No null function-pointer deleters.** The one-argument constructor is disabled when `D` is a function pointer, because a default-constructed one would be null.
- **Arrays get a separate interface.** `*` and `->` exist only for `T`, and `[]` only for `T[]`, selected with `requires` clauses. `MakeAutoRelease` rejects bounded arrays such as `int[5]`, as `std::make_unique` does.

### Known limitations

- There is no converting move from `AutoRelease<Derived>` to `AutoRelease<Base>`.
- Closure-type (lambda) deleters support construction and move construction, but not move assignment or `swap`, because closure types are not assignable.
- `Reset(p.Get())` is a precondition violation, as it is for `std::unique_ptr::reset`.

## UniqueResource

A move-only owner for a trivially copyable handle, with a per-object *sentinel* that means "no resource".

```cpp
#include "UniqueResource.h"

// OpenGL style: 0 means "no buffer"
UniqueResource<GLuint> vbo{0u, [](GLuint b) { glDeleteBuffers(1, &b); }};
glCreateBuffers(1, &vbo);                  // operator& exposes the handle as an out-parameter
glBindBuffer(GL_ARRAY_BUFFER, vbo);        // implicit conversion to the handle
```

### Design decisions

- **The sentinel is a constructor argument, not a template parameter**, so one type covers APIs that use `0`, `-1`, or `nullptr`. It always travels with the handle (moves and `Swap` carry it).
- **The deleter is a template parameter** (default `void(*)(T)`) rather than `std::function`. That avoids the size, allocation and indirect-call cost of type erasure, at the price of different deleters producing different types.
- **Move assignment uses move-and-swap.** The old resource is released by a temporary's destructor, so self-assignment needs no special guard.
- **C APIs work directly.** The implicit `operator T()` and `operator&` exist so the wrapper can be passed to and filled by C functions. As a consequence, `&wrapper` returns the handle's address, so use `std::addressof` to get the wrapper's own address. Only fill an *empty* wrapper through `&`, because a live handle would be overwritten without being released.

### Choosing the sentinel

Pick the value the API itself uses for "no resource": `0` for OpenGL names, `-1` for POSIX descriptors, `nullptr` for `FILE*`. A wrong sentinel silently leaks or releases a real handle.

## Defer

Scope guards that run a block of code when the enclosing scope ends, in the spirit of Go's `defer`. There are three variants, chosen by how the scope is left.

```cpp
#include "Defer.h"

void Process() {
    std::FILE* f = std::fopen("log.txt", "w");
    Defer(std::fclose(f));                        // always runs at scope exit

    BeginTransaction();
    DeferFail(Rollback());                        // runs only if an exception escapes the scope
    DeferSuccess(Commit());                       // runs only if the scope exits normally

    // ... risky work ...
}
```

| Macro | Runs when | Underlying type |
|---|---|---|
| `Defer(code)` | the scope exits, by any means | `ToyGround::DeferImpl` |
| `DeferFail(code)` | the scope exits because of an exception | `ToyGround::DeferImplScopeFail` |
| `DeferSuccess(code)` | the scope exits normally | `ToyGround::DeferImplScopeSuccess` |

### Design decisions

- **Macros create an anonymous guard.** Each macro wraps `code` in a `[&]` lambda and declares a uniquely named guard (`defer_0`, `defer_1`, …) using `__COUNTER__`, so several guards can live in one scope without naming them.
- **The callable is a template parameter**, deduced from the lambda (CTAD), rather than `std::function`. There is no allocation or indirect call.
- **Neither copyable nor movable.** A guard is bound to the scope it was declared in. Copying would run the cleanup twice, and moving it out would change when it runs.
- **Failure is detected with `std::uncaught_exceptions()`.** The count is recorded at construction and compared in the destructor, so the fail/success variants behave correctly even when constructed inside another object's destructor during stack unwinding (which the older `std::uncaught_exception()` could not handle).
- **Guards can be dismissed.** `Dismiss()` cancels the cleanup, for the "set up an undo, do the risky step, cancel the undo on success" pattern.
- **Cleanup runs in reverse order of declaration**, like any other local destructor, so later guards can rely on earlier resources still being alive.

### Using `Dismiss`

The macros hide the guard's name, so to call `Dismiss()` declare the guard directly:

```cpp
ToyGround::DeferImpl undo{[&] { RemoveTempFile(path); }};
WriteTempFile(path);
RenameIntoPlace(path);
undo.Dismiss();                                   // success: keep the file
```

### Known limitations

- The lambda captures everything by reference, so the guard must not outlive anything it uses (it can't, as long as it's only used through the macros).
- The cleanup code must not throw. Destructors are `noexcept`, so an exception escaping the guard calls `std::terminate`.
- `__COUNTER__` is a compiler extension, though GCC, Clang and MSVC all support it.
- `code` is a single macro argument, so a top-level comma outside parentheses (for example `std::map<int, int> m`) breaks it. Wrap such code in a function call or extra parentheses.

### When to use it

`Defer` is for one-off cleanup that doesn't deserve its own type. When the same resource is acquired in many places, prefer `AutoRelease` or `UniqueResource`, which also give ownership transfer and a reusable deleter.

## Tests

Each utility has a dependency-free test suite in `<Utility>/tests/`, with a small built-in harness that keeps running after a failure and prints a summary. The tests check two things:

- **Compile-time contracts**, such as `static_assert(!std::is_copy_constructible_v<...>)`, `noexcept` moves, `explicit` conversions, the absence of ordering operators, and `sizeof` checks.
- **Runtime behaviour**, using counting types and a fake C API (`UniqueResource/FakeApi.h`) to show that every resource is released exactly once, across moves, swaps, resets, arrays, and `std::vector` reallocation.

Run them under AddressSanitizer and UndefinedBehaviorSanitizer where available (not supported on MinGW).

## Layout

```
ToyGround/
├── xmake.lua              # shared settings; includes every */xmake.lua
├── AutoRelease/
│   ├── AutoRelease.h
│   ├── main.cpp           # example program
│   └── tests/tests.cpp
├── UniqueResource/
│   ├── UniqueResource.h
│   ├── FakeApi.h          # fake GL/POSIX handle API used by the tests
│   └── tests/tests.cpp
└── Defer/
    ├── Defer.h
    └── tests/tests.cpp
```
