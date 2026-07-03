#include <zukiru/memory/arena_allocator.hpp>

#include <zukiru/core/assert.hpp>

#include <cstdlib>
#include <utility>

namespace zukiru::memory {

ArenaAllocator::ArenaAllocator(usize capacityBytes)
    : buffer_(static_cast<byte*>(std::malloc(capacityBytes))),
      capacity_(buffer_ != nullptr ? capacityBytes : 0),
      ownsBuffer_(buffer_ != nullptr) {
    ZUKIRU_ENSURE_MSG(capacityBytes == 0 || buffer_ != nullptr, "arena: out of memory");
}

ArenaAllocator::ArenaAllocator(void* buffer, usize capacityBytes) noexcept
    : buffer_(static_cast<byte*>(buffer)), capacity_(buffer != nullptr ? capacityBytes : 0) {}

ArenaAllocator::~ArenaAllocator() {
    release();
}

void ArenaAllocator::release() noexcept {
    if (ownsBuffer_) std::free(buffer_);
    buffer_ = nullptr;
    capacity_ = 0;
    offset_ = 0;
    ownsBuffer_ = false;
}

ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
    : buffer_(other.buffer_),
      capacity_(other.capacity_),
      offset_(other.offset_),
      ownsBuffer_(other.ownsBuffer_) {
    other.buffer_ = nullptr;
    other.capacity_ = 0;
    other.offset_ = 0;
    other.ownsBuffer_ = false;
}

ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& other) noexcept {
    if (this != &other) {
        release();
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        offset_ = other.offset_;
        ownsBuffer_ = other.ownsBuffer_;
        other.buffer_ = nullptr;
        other.capacity_ = 0;
        other.offset_ = 0;
        other.ownsBuffer_ = false;
    }
    return *this;
}

void* ArenaAllocator::allocate(usize size, usize alignment) noexcept {
    ZUKIRU_ASSERT(isPowerOfTwo(alignment));
    if (buffer_ == nullptr || size == 0) return nullptr;

    const uptr base = reinterpret_cast<uptr>(buffer_);
    const uptr aligned = alignUp(base + offset_, alignment);
    const usize newOffset = (aligned - base) + size;
    if (newOffset > capacity_) return nullptr;  // exhausted

    offset_ = newOffset;
    return reinterpret_cast<void*>(aligned);
}

}  // namespace zukiru::memory
