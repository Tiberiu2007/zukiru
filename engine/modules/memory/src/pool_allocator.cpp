#include <zuki/memory/pool_allocator.hpp>

#include <zuki/core/assert.hpp>

#include <cstdlib>

namespace zuki::memory {
namespace {

// Round a requested block size up so it can hold a free-list pointer and meets
// the requested alignment.
usize normalizeBlockSize(usize blockSize, usize alignment) noexcept {
    const usize atLeastPtr = blockSize < sizeof(void*) ? sizeof(void*) : blockSize;
    return alignUp(atLeastPtr, alignment);
}

}  // namespace

PoolAllocator::PoolAllocator(usize blockSize, usize blockCount, usize alignment)
    : blockSize_(normalizeBlockSize(blockSize, alignment)), blockCount_(blockCount) {
    ZUKI_ENSURE_MSG(isPowerOfTwo(alignment), "pool: alignment must be a power of two");
    if (blockCount_ == 0) return;

    // Over-allocate by `alignment` so the block region can be aligned within it.
    const usize bytes = blockSize_ * blockCount_;
    rawBuffer_ = std::malloc(bytes + alignment);
    ZUKI_ENSURE_MSG(rawBuffer_ != nullptr, "pool: out of memory");
    blocks_ = static_cast<byte*>(alignPointer(rawBuffer_, alignment));
    ownsBuffer_ = true;
    buildFreeList();
}

PoolAllocator::PoolAllocator(void* buffer, usize blockSize, usize blockCount,
                             usize alignment) noexcept
    : blocks_(static_cast<byte*>(buffer)),
      blockSize_(normalizeBlockSize(blockSize, alignment)),
      blockCount_(buffer != nullptr ? blockCount : 0) {
    if (blocks_ != nullptr && blockCount_ > 0) buildFreeList();
}

PoolAllocator::~PoolAllocator() {
    release();
}

void PoolAllocator::release() noexcept {
    if (ownsBuffer_) std::free(rawBuffer_);
    rawBuffer_ = nullptr;
    blocks_ = nullptr;
    freeList_ = nullptr;
    blockCount_ = 0;
    allocatedBlocks_ = 0;
    ownsBuffer_ = false;
}

void PoolAllocator::buildFreeList() noexcept {
    // Link blocks front-to-back so allocate() returns them in ascending order.
    freeList_ = nullptr;
    for (usize i = blockCount_; i-- > 0;) {
        byte* block = blocks_ + i * blockSize_;
        *reinterpret_cast<void**>(block) = freeList_;
        freeList_ = block;
    }
    allocatedBlocks_ = 0;
}

PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
    : rawBuffer_(other.rawBuffer_),
      blocks_(other.blocks_),
      freeList_(other.freeList_),
      blockSize_(other.blockSize_),
      blockCount_(other.blockCount_),
      allocatedBlocks_(other.allocatedBlocks_),
      ownsBuffer_(other.ownsBuffer_) {
    other.rawBuffer_ = nullptr;
    other.blocks_ = nullptr;
    other.freeList_ = nullptr;
    other.blockCount_ = 0;
    other.allocatedBlocks_ = 0;
    other.ownsBuffer_ = false;
}

PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept {
    if (this != &other) {
        release();
        rawBuffer_ = other.rawBuffer_;
        blocks_ = other.blocks_;
        freeList_ = other.freeList_;
        blockSize_ = other.blockSize_;
        blockCount_ = other.blockCount_;
        allocatedBlocks_ = other.allocatedBlocks_;
        ownsBuffer_ = other.ownsBuffer_;
        other.rawBuffer_ = nullptr;
        other.blocks_ = nullptr;
        other.freeList_ = nullptr;
        other.blockCount_ = 0;
        other.allocatedBlocks_ = 0;
        other.ownsBuffer_ = false;
    }
    return *this;
}

void* PoolAllocator::allocate() noexcept {
    if (freeList_ == nullptr) return nullptr;
    void* block = freeList_;
    freeList_ = *reinterpret_cast<void**>(freeList_);
    ++allocatedBlocks_;
    return block;
}

void PoolAllocator::free(void* block) noexcept {
    if (block == nullptr) return;
    ZUKI_ASSERT_MSG(allocatedBlocks_ > 0, "pool: free() with no live blocks");
    *reinterpret_cast<void**>(block) = freeList_;
    freeList_ = block;
    --allocatedBlocks_;
}

void PoolAllocator::reset() noexcept {
    if (blocks_ != nullptr) buildFreeList();
}

}  // namespace zuki::memory
