#include <zuki/math/scalar.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zuki::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("angle conversion round-trips", "[math][scalar]") {
    STATIC_REQUIRE(radians(180.0f) > 3.14f);
    REQUIRE_THAT(degrees(radians(90.0f)), WithinAbs(90.0f, 1e-4f));
    REQUIRE_THAT(radians(180.0f), WithinAbs(kPi, 1e-5f));
}

TEST_CASE("clamp / saturate / sign", "[math][scalar]") {
    STATIC_REQUIRE(clamp(5, 0, 3) == 3);
    STATIC_REQUIRE(clamp(-1, 0, 3) == 0);
    STATIC_REQUIRE(clamp(2, 0, 3) == 2);
    REQUIRE(saturate(1.5f) == 1.0f);
    REQUIRE(saturate(-0.5f) == 0.0f);
    REQUIRE(sign(-3.0f) == -1.0f);
    REQUIRE(sign(0.0f) == 0.0f);
}

TEST_CASE("lerp interpolates", "[math][scalar]") {
    REQUIRE(lerp(0.0f, 10.0f, 0.5f) == 5.0f);
    REQUIRE(lerp(2.0f, 4.0f, 0.0f) == 2.0f);
    REQUIRE(lerp(2.0f, 4.0f, 1.0f) == 4.0f);
}

TEST_CASE("approx helpers", "[math][scalar]") {
    REQUIRE(approxEqual(1.0f, 1.0f + 1e-8f));
    REQUIRE_FALSE(approxEqual(1.0f, 1.1f));
    REQUIRE(approxZero(1e-8f));
}
