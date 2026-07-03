// Thread-related OS utilities: core count, thread ids, naming, and yielding.
// (A full job system is a Layer-1 concern — see the `jobs` module. This is the
// thin OS surface it and others build on.)
#pragma once

#include <zukiru/core/types.hpp>

#include <string>
#include <string_view>

namespace zukiru::platform {

// Number of hardware threads available (never returns 0; falls back to 1).
[[nodiscard]] u32 hardwareConcurrency() noexcept;

// A stable identifier for the calling thread within this process.
[[nodiscard]] u64 currentThreadId() noexcept;

// Hint to the scheduler that the calling thread can give up its slice.
void yieldThread() noexcept;

// Set/get the calling thread's debugger-visible name. Names are truncated to
// the platform limit (15 chars on Linux). Returns false if unsupported on this
// platform. Currently implemented on POSIX (Linux/macOS).
bool setThreadName(std::string_view name);
[[nodiscard]] std::string threadName();

}  // namespace zukiru::platform
