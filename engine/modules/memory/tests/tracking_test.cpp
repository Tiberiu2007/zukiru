#include <zuki/memory/tracking.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zuki::memory;

TEST_CASE("tracker accounts for live bytes and counts", "[memory][tracking]") {
    MemoryTracker tracker;
    tracker.recordAllocation(100);
    tracker.recordAllocation(50);
    REQUIRE(tracker.bytesInUse() == 150);
    REQUIRE(tracker.liveAllocations() == 2);
    REQUIRE(tracker.totalAllocations() == 2);

    tracker.recordDeallocation(100);
    REQUIRE(tracker.bytesInUse() == 50);
    REQUIRE(tracker.liveAllocations() == 1);
    REQUIRE(tracker.totalAllocations() == 2);  // total never decreases
}

TEST_CASE("peak tracks the high-water mark", "[memory][tracking]") {
    MemoryTracker tracker;
    tracker.recordAllocation(200);
    tracker.recordAllocation(300);  // peak 500
    tracker.recordDeallocation(300);
    tracker.recordAllocation(100);  // now 300, peak stays 500
    REQUIRE(tracker.bytesInUse() == 300);
    REQUIRE(tracker.peakBytes() == 500);
    REQUIRE(tracker.totalBytesAllocated() == 600);
}

TEST_CASE("reset clears everything", "[memory][tracking]") {
    MemoryTracker tracker;
    tracker.recordAllocation(1000);
    tracker.reset();
    REQUIRE(tracker.bytesInUse() == 0);
    REQUIRE(tracker.peakBytes() == 0);
    REQUIRE(tracker.liveAllocations() == 0);
    REQUIRE(tracker.totalAllocations() == 0);
}
