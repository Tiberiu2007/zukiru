#include <zuki/memory/stack_allocator.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zuki;
using namespace zuki::memory;

TEST_CASE("stack allocates and frees to a marker (LIFO)", "[memory][stack]") {
    StackAllocator stack(1024);
    (void)stack.allocate(64, 1);
    const StackAllocator::Marker mark = stack.mark();
    const usize usedAtMark = stack.used();

    (void)stack.allocate(128, 1);
    (void)stack.allocate(64, 1);
    REQUIRE(stack.used() > usedAtMark);

    stack.freeToMarker(mark);
    REQUIRE(stack.used() == usedAtMark);

    // Space is reusable after rollback.
    void* p = stack.allocate(128, 1);
    REQUIRE(p != nullptr);
}

TEST_CASE("nested markers roll back independently", "[memory][stack]") {
    StackAllocator stack(1024);
    const auto outer = stack.mark();
    (void)stack.allocate(100, 1);
    const auto inner = stack.mark();
    (void)stack.allocate(100, 1);

    stack.freeToMarker(inner);
    REQUIRE(stack.used() == 100);
    stack.freeToMarker(outer);
    REQUIRE(stack.used() == 0);
}

TEST_CASE("stack respects capacity", "[memory][stack]") {
    StackAllocator stack(64);
    REQUIRE(stack.allocate(64, 1) != nullptr);
    REQUIRE(stack.allocate(1, 1) == nullptr);
}

TEST_CASE("freeToMarker ahead of the top is a no-op", "[memory][stack]") {
    StackAllocator stack(128);
    (void)stack.allocate(16, 1);
    const auto here = stack.mark();
    stack.freeToMarker(here + 999);  // marker beyond current top: ignored
    REQUIRE(stack.used() == 16);
}
