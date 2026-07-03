#include <zukiru/platform/clock.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zukiru;
using namespace zukiru::platform;

TEST_CASE("performance counter is monotonic and advances", "[platform][clock]") {
    const u64 a = performanceCounter();
    const u64 b = performanceCounter();
    REQUIRE(b >= a);
    REQUIRE(performanceFrequency() == 1'000'000'000ull);
}

TEST_CASE("sleepFor advances the performance counter", "[platform][clock]") {
    const u64 start = performanceCounter();
    sleepFor(Duration::fromMillis(5));
    const u64 elapsedNs = performanceCounter() - start;
    // Allow generous scheduling slack, but it must have slept a bit.
    REQUIRE(elapsedNs >= 1'000'000ull);  // >= 1ms
}

TEST_CASE("wall-clock time is plausible", "[platform][clock]") {
    const u64 seconds = unixTimeSeconds();
    const u64 millis = unixTimeMilliseconds();
    // After 2020-01-01 (1'577'836'800). Sanity that the clock is wired up.
    REQUIRE(seconds > 1'577'836'800ull);
    REQUIRE(millis / 1000 >= seconds - 2);
}
