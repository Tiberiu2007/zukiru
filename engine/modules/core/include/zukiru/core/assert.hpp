// Assertions, panics and the customizable failure handler.
//
//   ZUKIRU_ASSERT(cond)            debug-only, compiled out in release
//   ZUKIRU_ASSERT_MSG(cond, msg)   "
//   ZUKIRU_ASSERTF(cond, fmt, ...) " with std::format message
//   ZUKIRU_ENSURE(cond[, msg])     ALWAYS active (even in release)
//   ZUKIRU_PANIC(msg)              unconditional failure
//   ZUKIRU_PANICF(fmt, ...)        "                    with std::format message
//   ZUKIRU_UNREACHABLE()           marks an impossible path
//
// On failure the active AssertHandler is invoked; the default prints to stderr
// and aborts. Tests can install a handler that throws to observe failures
// without terminating (see setAssertHandler).
#pragma once

#include <zukiru/core/types.hpp>

#include <format>
#include <string_view>
#include <utility>

namespace zukiru {

// Where an assertion fired. Cheap to pass by value.
struct SourceLocation {
    const char* file = "";
    const char* function = "";
    i32 line = 0;
};

// Called when an assertion/panic fires. `expr` is the stringized condition (may
// be empty for a bare panic); `message` is the optional user text.
using AssertHandler = void (*)(const SourceLocation& where, std::string_view expr,
                               std::string_view message);

// Install a new handler; returns the previous one. Passing nullptr restores the
// default (print-and-abort) handler. Not thread-safe with concurrent failures.
AssertHandler setAssertHandler(AssertHandler handler);

namespace detail {

// Invokes the active handler, then aborts if it returns (a throwing handler is
// allowed and satisfies [[noreturn]]).
[[noreturn]] void assertFail(const SourceLocation& where, std::string_view expr,
                             std::string_view message);

}  // namespace detail

}  // namespace zukiru

// --- Portable helpers -----------------------------------------------------
#if defined(_MSC_VER)
#define ZUKIRU_FUNCTION __FUNCSIG__
#else
#define ZUKIRU_FUNCTION __PRETTY_FUNCTION__
#endif

#define ZUKIRU_SOURCE_LOCATION \
    ::zukiru::SourceLocation { __FILE__, ZUKIRU_FUNCTION, __LINE__ }

// Always-active check (validation of external input, invariants that matter in
// shipping builds). Never compiled out.
#define ZUKIRU_ENSURE(cond)                                                          \
    do {                                                                           \
        if (!(cond)) [[unlikely]] {                                                \
            ::zukiru::detail::assertFail(ZUKIRU_SOURCE_LOCATION, #cond, {});           \
        }                                                                          \
    } while (false)

#define ZUKIRU_ENSURE_MSG(cond, msg)                                                 \
    do {                                                                           \
        if (!(cond)) [[unlikely]] {                                                \
            ::zukiru::detail::assertFail(ZUKIRU_SOURCE_LOCATION, #cond, (msg));        \
        }                                                                          \
    } while (false)

#define ZUKIRU_PANIC(msg) ::zukiru::detail::assertFail(ZUKIRU_SOURCE_LOCATION, {}, (msg))

#define ZUKIRU_PANICF(...) \
    ::zukiru::detail::assertFail(ZUKIRU_SOURCE_LOCATION, {}, ::std::format(__VA_ARGS__))

#if defined(ZUKIRU_DEBUG) && ZUKIRU_DEBUG
#define ZUKIRU_ASSERT(cond) ZUKIRU_ENSURE(cond)
#define ZUKIRU_ASSERT_MSG(cond, msg) ZUKIRU_ENSURE_MSG(cond, msg)
#define ZUKIRU_ASSERTF(cond, ...)                                                    \
    do {                                                                           \
        if (!(cond)) [[unlikely]] {                                                \
            ::zukiru::detail::assertFail(ZUKIRU_SOURCE_LOCATION, #cond,                \
                                       ::std::format(__VA_ARGS__));                \
        }                                                                          \
    } while (false)
#define ZUKIRU_UNREACHABLE() ZUKIRU_PANIC("reached code marked unreachable")
#else
// Compiled out in release, but still parse `cond` so it can't rot.
#define ZUKIRU_ASSERT(cond) ((void)sizeof(cond))
#define ZUKIRU_ASSERT_MSG(cond, msg) ((void)sizeof(cond))
#define ZUKIRU_ASSERTF(cond, ...) ((void)sizeof(cond))
#if defined(_MSC_VER)
#define ZUKIRU_UNREACHABLE() __assume(false)
#else
#define ZUKIRU_UNREACHABLE() __builtin_unreachable()
#endif
#endif
