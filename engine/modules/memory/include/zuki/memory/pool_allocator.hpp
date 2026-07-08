// PoolAllocator — fixed-size block allocator. Every allocate()/free() is O(1)
// and never fragments: the pool hands out equal-sized blocks and threads a
// free list through the unused ones. Perfect for many same-sized objects
// (components, particles, nodes).
//
// The requested block size is rounded up to hold a free-list pointer and to the
// requested alignment.
#pragma once

#include <zuki/core/types.hpp>
#include <zuki/memory/alignment.hpp>

namespace zuki::memory {

class PoolAllocator {
public:
    PoolAllocator() = default;

    // Own a heap buffer sized for `blockCount` blocks of (at least) `blockSize`.
    PoolAllocator(usize blockSize, usize blockCount, usize alignment = kDefaultAlignment);

    // Borrow an external buffer. It must be at least blockSize()*blockCount bytes
    // and aligned to `alignment`.
    PoolAllocator(void* buffer, usize blockSize, usize blockCount,
                  usize alignment = kDefaultAlignment) noexcept;

    ~PoolAllocator();

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&& other) noexcept;
    PoolAllocator& operator=(PoolAllocator&& other) noexcept;

    // Grab one block, or nullptr if the pool is full.
    [[nodiscard]] void* allocate() noexcept;

    // Return a block previously handed out by this pool.
    void free(void* block) noexcept;

    // Return all blocks to the pool (invalidates outstanding pointers).
    void reset() noexcept;

    [[nodiscard]] usize blockSize() const noexcept { return blockSize_; }
    [[nodiscard]] usize capacity() const noexcept { return blockCount_; }
    [[nodiscard]] usize allocatedBlocks() const noexcept { return allocatedBlocks_; }
    [[nodiscard]] usize freeBlocks() const noexcept { return blockCount_ - allocatedBlocks_; }

private:
    void buildFreeList() noexcept;
    void release() noexcept;

    void* rawBuffer_ = nullptr;  // original allocation to free (owning only)
    byte* blocks_ = nullptr;     // aligned start of the block region
    void* freeList_ = nullptr;   // head of the intrusive free list
    usize blockSize_ = 0;
    usize blockCount_ = 0;
    usize allocatedBlocks_ = 0;
    bool ownsBuffer_ = false;
};

}  // namespace zuki::memory
