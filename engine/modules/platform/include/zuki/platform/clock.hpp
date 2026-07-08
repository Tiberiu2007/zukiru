// OS timing primitives: sleeping, a high-resolution performance counter, and
// wall-clock time. (Monotonic Instants for gameplay timing live in core/time;
// this is the lower-level OS-facing surface.)
#pragma once

#include <zuki/core/time.hpp>
#include <zuki/core/types.hpp>

namespace zuki::platform {

// Block the calling thread for approximately `duration` (subject to OS scheduling
// granularity). Negative/zero durations return immediately.
void sleepFor(Duration duration);
void sleepMilliseconds(u32 milliseconds);

// A monotonically increasing, high-resolution tick count and its rate. Use
// deltas of performanceCounter() divided by performanceFrequency() for precise
// interval timing / profiling.
[[nodiscard]] u64 performanceCounter() noexcept;
[[nodiscard]] u64 performanceFrequency() noexcept;

// Wall-clock time since the Unix epoch (subject to system clock changes; do not
// use for measuring intervals — use the performance counter for that).
[[nodiscard]] u64 unixTimeSeconds() noexcept;
[[nodiscard]] u64 unixTimeMilliseconds() noexcept;

}  // namespace zuki::platform
