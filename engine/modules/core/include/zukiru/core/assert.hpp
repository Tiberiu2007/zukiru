// Assertions, panics and the customizable failure handler.
//
//   ZUKI_ASSERT(cond)            debug-only, compiled out in release
//   ZUKI_ASSERT_MSG(cond, msg)   "
//   ZUKI_ASSERTF(cond, fmt, ...) " with std::format message
//   ZUKI_ENSURE(cond[, msg])     ALWAYS active (even in release)
//   ZUKI_PANIC(msg)              unconditional failure
//   ZUKI_PANICF(fmt, ...)        "                    with std::format message
//   ZUKI_UNREACHABLE()           marks an impossible path
//
// On failure the active AssertHandler is invoked; the default prints to stderr
// and aborts. Tests can install a handler that throws to observe failures
// without terminating (see setAssertHandler).
#pragma once

#include <zukiru/core/types.hpp>

#include <format>
#include <string_view>
#include <utility>

namespace zuki {

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

}  // namespace zuki

// --- Portable helpers -----------------------------------------------------
#if defined(_MSC_VER)
#define ZUKI_FUNCTION __FUNCSIG__
#else
#define ZUKI_FUNCTION __PRETTY_FUNCTION__
#endif

#define ZUKI_SOURCE_LOCATION \
    ::zuki::SourceLocation { __FILE__, ZUKI_FUNCTION, __LINE__ }

// Always-active check (validation of external input, invariants that matter in
// shipping builds). Never compiled out.
#define ZUKI_ENSURE(cond)                                                          \
    do {                                                                           \
        if (!(cond)) [[unlikely]] {                                                \
            ::zuki::detail::assertFail(ZUKI_SOURCE_LOCATION, #cond, {});           \
        }                                                                          \
    } while (false)

#define ZUKI_ENSURE_MSG(cond, msg)                                                 \
    do {                                                                           \
        if (!(cond)) [[unlikely]] {                                                \
            ::zuki::detail::assertFail(ZUKI_SOURCE_LOCATION, #cond, (msg));        \
        }                                                                          \
    } while (false)

#define ZUKI_PANIC(msg) ::zuki::detail::assertFail(ZUKI_SOURCE_LOCATION, {}, (msg))

#define ZUKI_PANICF(...) \
    ::zuki::detail::assertFail(ZUKI_SOURCE_LOCATION, {}, ::std::format(__VA_ARGS__))

#if defined(ZUKI_DEBUG) && ZUKI_DEBUG
#define ZUKI_ASSERT(cond) ZUKI_ENSURE(cond)
#define ZUKI_ASSERT_MSG(cond, msg) ZUKI_ENSURE_MSG(cond, msg)
#define ZUKI_ASSERTF(cond, ...)                                                    \
    do {                                                                           \
        if (!(cond)) [[unlikely]] {                                                \
            ::zuki::detail::assertFail(ZUKI_SOURCE_LOCATION, #cond,                \
                                       ::std::format(__VA_ARGS__));                \
        }                                                                          \
    } while (false)
#define ZUKI_UNREACHABLE() ZUKI_PANIC("reached code marked unreachable")
#else
// Compiled out in release, but still parse `cond` so it can't rot.
#define ZUKI_ASSERT(cond) ((void)sizeof(cond))
#define ZUKI_ASSERT_MSG(cond, msg) ((void)sizeof(cond))
#define ZUKI_ASSERTF(cond, ...) ((void)sizeof(cond))
#if defined(_MSC_VER)
#define ZUKI_UNREACHABLE() __assume(false)
#else
#define ZUKI_UNREACHABLE() __builtin_unreachable()
#endif
#endif
