#include <zuki/memory/arena_allocator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace zuki;
using namespace zuki::memory;

TEST_CASE("arena hands out advancing, aligned blocks", "[memory][arena]") {
    ArenaAllocator arena(1024);
    REQUIRE(arena.capacity() == 1024);
    REQUIRE(arena.used() == 0);

    void* a = arena.allocate(16, 16);
    void* b = arena.allocate(32, 16);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(isAligned(reinterpret_cast<uptr>(a), 16));
    REQUIRE(isAligned(reinterpret_cast<uptr>(b), 16));
    REQUIRE(b > a);
    REQUIRE(arena.used() >= 48);
}

TEST_CASE("arena returns nullptr when exhausted", "[memory][arena]") {
    ArenaAllocator arena(64);
    REQUIRE(arena.allocate(64, 1) != nullptr);
    REQUIRE(arena.allocate(1, 1) == nullptr);  // full
    REQUIRE(arena.remaining() == 0);
}

TEST_CASE("arena reset reclaims everything", "[memory][arena]") {
    ArenaAllocator arena(128);
    (void)arena.allocate(100, 1);
    REQUIRE(arena.used() == 100);
    arena.reset();
    REQUIRE(arena.used() == 0);
    REQUIRE(arena.allocate(100, 1) != nullptr);
}

TEST_CASE("typed allocate returns aligned storage", "[memory][arena]") {
    ArenaAllocator arena(256);
    auto* values = arena.allocate<double>(4);
    REQUIRE(values != nullptr);
    REQUIRE(isAligned(reinterpret_cast<uptr>(values), alignof(double)));
    for (int i = 0; i < 4; ++i) values[i] = i * 1.5;
    REQUIRE(values[3] == 4.5);
}

TEST_CASE("arena can borrow an external buffer", "[memory][arena]") {
    std::array<unsigned char, 256> storage{};
    ArenaAllocator arena(storage.data(), storage.size());
    void* p = arena.allocate(64, 1);
    REQUIRE(p != nullptr);
    REQUIRE(p >= storage.data());
}

TEST_CASE("arena is movable", "[memory][arena]") {
    ArenaAllocator a(128);
    (void)a.allocate(32, 1);
    ArenaAllocator b(std::move(a));
    REQUIRE(b.used() == 32);
    REQUIRE(b.capacity() == 128);
}
