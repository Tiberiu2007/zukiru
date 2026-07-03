#include <zukiru/core/time.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zukiru;
using Catch::Matchers::WithinAbs;

TEST_CASE("Duration converts between units", "[core][time]") {
    const Duration d = Duration::fromMillis(1500);
    REQUIRE_THAT(d.seconds(), WithinAbs(1.5, 1e-9));
    REQUIRE_THAT(d.millis(), WithinAbs(1500.0, 1e-6));
    REQUIRE(d.nanos() == 1'500'000'000);
}

TEST_CASE("Duration arithmetic and comparison", "[core][time]") {
    const Duration a = Duration::fromSeconds(2);
    const Duration b = Duration::fromSeconds(3);
    REQUIRE((a + b).seconds() == 5.0);
    REQUIRE((b - a).seconds() == 1.0);
    REQUIRE(a < b);
    REQUIRE(a != b);

    Duration c = Duration::fromMillis(500);
    c += Duration::fromMillis(500);
    REQUIRE(c.seconds() == 1.0);
}

TEST_CASE("Clock is monotonic", "[core][time]") {
    const Instant t0 = Clock::now();
    const Instant t1 = Clock::now();
    REQUIRE(t1 >= t0);
    REQUIRE((t1 - t0).nanos() >= 0);
}

TEST_CASE("Instant::elapsed is non-negative", "[core][time]") {
    const Instant start = Clock::now();
    REQUIRE(start.elapsed().nanos() >= 0);
}

TEST_CASE("Stopwatch measures and laps", "[core][time]") {
    Stopwatch sw;
    REQUIRE(sw.elapsed().nanos() >= 0);
    const Duration lap = sw.lap();
    REQUIRE(lap.nanos() >= 0);
    // After a lap the stopwatch restarts, so a fresh reading is small/non-negative.
    REQUIRE(sw.elapsed().nanos() >= 0);
}
