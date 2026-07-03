#include <zukiru/memory/alignment.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zukiru;
using namespace zukiru::memory;

TEST_CASE("isPowerOfTwo", "[memory][alignment]") {
    STATIC_REQUIRE(isPowerOfTwo(1));
    STATIC_REQUIRE(isPowerOfTwo(16));
    STATIC_REQUIRE(isPowerOfTwo(4096));
    STATIC_REQUIRE_FALSE(isPowerOfTwo(0));
    STATIC_REQUIRE_FALSE(isPowerOfTwo(3));
    STATIC_REQUIRE_FALSE(isPowerOfTwo(48));
}

TEST_CASE("alignUp rounds up to the next multiple", "[memory][alignment]") {
    STATIC_REQUIRE(alignUp(0, 16) == 0);
    STATIC_REQUIRE(alignUp(1, 16) == 16);
    STATIC_REQUIRE(alignUp(16, 16) == 16);
    STATIC_REQUIRE(alignUp(17, 16) == 32);
    STATIC_REQUIRE(alignUp(100, 8) == 104);
}

TEST_CASE("alignDown and isAligned", "[memory][alignment]") {
    STATIC_REQUIRE(alignDown(17, 16) == 16);
    STATIC_REQUIRE(alignDown(16, 16) == 16);
    STATIC_REQUIRE(isAligned(32, 16));
    STATIC_REQUIRE_FALSE(isAligned(17, 16));
}

TEST_CASE("alignPointer produces an aligned address", "[memory][alignment]") {
    alignas(64) unsigned char buffer[128];
    void* p = buffer + 1;  // deliberately misaligned
    void* aligned = alignPointer(p, 16);
    REQUIRE(isAligned(reinterpret_cast<uptr>(aligned), 16));
    REQUIRE(aligned >= p);
}
