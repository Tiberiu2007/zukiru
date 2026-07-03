#include <zukiru/memory/pool_allocator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <vector>

using namespace zukiru;
using namespace zukiru::memory;

TEST_CASE("pool block size is rounded up for pointer + alignment", "[memory][pool]") {
    PoolAllocator pool(1, 4);  // 1-byte request rounds up to >= sizeof(void*)
    REQUIRE(pool.blockSize() >= sizeof(void*));
    REQUIRE(pool.capacity() == 4);
    REQUIRE(pool.freeBlocks() == 4);
}

TEST_CASE("pool allocates distinct aligned blocks until full", "[memory][pool]") {
    PoolAllocator pool(32, 3, 16);
    std::set<void*> seen;
    for (int i = 0; i < 3; ++i) {
        void* p = pool.allocate();
        REQUIRE(p != nullptr);
        REQUIRE(isAligned(reinterpret_cast<uptr>(p), 16));
        REQUIRE(seen.insert(p).second);  // unique
    }
    REQUIRE(pool.allocatedBlocks() == 3);
    REQUIRE(pool.allocate() == nullptr);  // full
}

TEST_CASE("freed blocks are recycled", "[memory][pool]") {
    PoolAllocator pool(32, 2);
    void* a = pool.allocate();
    void* b = pool.allocate();
    REQUIRE(pool.allocate() == nullptr);

    pool.free(a);
    REQUIRE(pool.freeBlocks() == 1);
    void* c = pool.allocate();
    REQUIRE(c == a);  // most-recently-freed reused
    REQUIRE(pool.allocatedBlocks() == 2);
    (void)b;
    (void)c;
}

TEST_CASE("pool blocks are writable and non-overlapping", "[memory][pool]") {
    PoolAllocator pool(sizeof(u64), 8);
    std::vector<u64*> ptrs;
    for (int i = 0; i < 8; ++i) {
        auto* p = static_cast<u64*>(pool.allocate());
        REQUIRE(p != nullptr);
        *p = static_cast<u64>(i);
        ptrs.push_back(p);
    }
    for (int i = 0; i < 8; ++i) REQUIRE(*ptrs[static_cast<usize>(i)] == static_cast<u64>(i));
}

TEST_CASE("reset returns all blocks", "[memory][pool]") {
    PoolAllocator pool(16, 4);
    (void)pool.allocate();
    (void)pool.allocate();
    REQUIRE(pool.allocatedBlocks() == 2);
    pool.reset();
    REQUIRE(pool.allocatedBlocks() == 0);
    REQUIRE(pool.freeBlocks() == 4);
}

TEST_CASE("free(nullptr) is ignored", "[memory][pool]") {
    PoolAllocator pool(16, 2);
    pool.free(nullptr);
    REQUIRE(pool.allocatedBlocks() == 0);
}
