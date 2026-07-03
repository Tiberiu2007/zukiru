// Time primitives: a Duration value type, monotonic Instants, a Clock, and a
// Stopwatch. Built on std::chrono's steady_clock so measurements are immune to
// wall-clock adjustments.
#pragma once

#include <zukiru/core/types.hpp>

#include <chrono>

namespace zukiru {

// A signed span of time, stored as nanoseconds. Cheap value type.
class Duration {
public:
    constexpr Duration() = default;

    [[nodiscard]] static constexpr Duration fromNanos(i64 ns) { return Duration{ns}; }
    [[nodiscard]] static constexpr Duration fromMicros(f64 us) {
        return Duration{static_cast<i64>(us * 1'000.0)};
    }
    [[nodiscard]] static constexpr Duration fromMillis(f64 ms) {
        return Duration{static_cast<i64>(ms * 1'000'000.0)};
    }
    [[nodiscard]] static constexpr Duration fromSeconds(f64 s) {
        return Duration{static_cast<i64>(s * 1'000'000'000.0)};
    }

    [[nodiscard]] constexpr i64 nanos() const noexcept { return ns_; }
    [[nodiscard]] constexpr f64 micros() const noexcept { return static_cast<f64>(ns_) / 1e3; }
    [[nodiscard]] constexpr f64 millis() const noexcept { return static_cast<f64>(ns_) / 1e6; }
    [[nodiscard]] constexpr f64 seconds() const noexcept { return static_cast<f64>(ns_) / 1e9; }

    constexpr Duration& operator+=(Duration o) noexcept {
        ns_ += o.ns_;
        return *this;
    }
    constexpr Duration& operator-=(Duration o) noexcept {
        ns_ -= o.ns_;
        return *this;
    }

    friend constexpr Duration operator+(Duration a, Duration b) noexcept {
        return Duration{a.ns_ + b.ns_};
    }
    friend constexpr Duration operator-(Duration a, Duration b) noexcept {
        return Duration{a.ns_ - b.ns_};
    }
    friend constexpr auto operator<=>(Duration, Duration) = default;
    friend constexpr bool operator==(Duration, Duration) = default;

private:
    explicit constexpr Duration(i64 ns) : ns_(ns) {}
    i64 ns_ = 0;
};

// A point on the monotonic clock. Meaningful only via differences / elapsed().
class Instant {
public:
    Instant() = default;

    // Time elapsed since this instant was captured.
    [[nodiscard]] Duration elapsed() const;

    // Duration between two instants (this - earlier).
    [[nodiscard]] Duration operator-(Instant earlier) const {
        return Duration::fromNanos(
            std::chrono::duration_cast<std::chrono::nanoseconds>(point_ - earlier.point_).count());
    }

    friend constexpr auto operator<=>(const Instant&, const Instant&) = default;

private:
    friend class Clock;
    using SteadyPoint = std::chrono::steady_clock::time_point;
    explicit Instant(SteadyPoint p) : point_(p) {}
    SteadyPoint point_{};
};

// Source of monotonic Instants.
class Clock {
public:
    [[nodiscard]] static Instant now();
};

// Measures elapsed time. Starts running on construction.
class Stopwatch {
public:
    Stopwatch() : start_(Clock::now()) {}

    void reset() { start_ = Clock::now(); }

    [[nodiscard]] Duration elapsed() const { return start_.elapsed(); }

    // Elapsed time so far, then restart from now (a lap timer).
    [[nodiscard]] Duration lap() {
        const Instant now = Clock::now();
        const Duration d = now - start_;
        start_ = now;
        return d;
    }

private:
    Instant start_;
};

}  // namespace zukiru
