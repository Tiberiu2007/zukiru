// Build-pipeline smoke test. Proves the toolchain end-to-end: CMake configure,
// Catch2 acquisition, C++20 compile, link, and CTest discovery all work.
//
// This deliberately depends on no engine module (there are none yet — those
// arrive in Milestone 1). It only exercises the build machinery itself.

#include <catch2/catch_test_macros.hpp>

#include <string_view>

namespace {

// A trivially constexpr-evaluable helper: also confirms C++20 is really on.
constexpr int addUp(int a, int b) noexcept {
    return a + b;
}

}  // namespace

TEST_CASE("build pipeline is alive", "[smoke]") {
    STATIC_REQUIRE(addUp(2, 2) == 4);
    REQUIRE(addUp(40, 2) == 42);
}

TEST_CASE("C++20 language features compile", "[smoke]") {
    // constinit + consteval-ish usage and std::string_view literals.
    using namespace std::literals;
    constexpr auto name = "zuki"sv;
    REQUIRE(name.size() == 4);
    REQUIRE(name.starts_with("zuki"));  // C++20 member
}
