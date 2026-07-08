#include <zuki/core/types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace zuki;

TEST_CASE("fixed-width aliases have the advertised sizes", "[core][types]") {
    STATIC_REQUIRE(sizeof(i8) == 1);
    STATIC_REQUIRE(sizeof(u16) == 2);
    STATIC_REQUIRE(sizeof(i32) == 4);
    STATIC_REQUIRE(sizeof(u64) == 8);
    STATIC_REQUIRE(sizeof(f32) == 4);
    STATIC_REQUIRE(sizeof(f64) == 8);
    STATIC_REQUIRE(std::is_signed_v<i32>);
    STATIC_REQUIRE(std::is_unsigned_v<u32>);
}

TEST_CASE("size / pointer types match the platform", "[core][types]") {
    STATIC_REQUIRE(sizeof(usize) == sizeof(void*));
    STATIC_REQUIRE(sizeof(uptr) == sizeof(void*));
    STATIC_REQUIRE(std::is_same_v<usize, std::size_t>);
}

TEST_CASE("integer literals are usize/u32/u64", "[core][types]") {
    STATIC_REQUIRE(std::is_same_v<decltype(10_uz), usize>);
    STATIC_REQUIRE(std::is_same_v<decltype(10_u32), u32>);
    STATIC_REQUIRE(std::is_same_v<decltype(10_u64), u64>);
    REQUIRE(5_uz + 5_uz == 10_uz);
}
