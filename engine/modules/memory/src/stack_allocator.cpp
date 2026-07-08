#include <zuki/memory/stack_allocator.hpp>

#include <zuki/core/assert.hpp>

#include <cstdlib>

namespace zuki::memory {

StackAllocator::StackAllocator(usize capacityBytes)
    : buffer_(static_cast<byte*>(std::malloc(capacityBytes))),
      capacity_(buffer_ != nullptr ? capacityBytes : 0),
      ownsBuffer_(buffer_ != nullptr) {
    ZUKI_ENSURE_MSG(capacityBytes == 0 || buffer_ != nullptr, "stack allocator: out of memory");
}

StackAllocator::StackAllocator(void* buffer, usize capacityBytes) noexcept
    : buffer_(static_cast<byte*>(buffer)), capacity_(buffer != nullptr ? capacityBytes : 0) {}

StackAllocator::~StackAllocator() {
    release();
}

void StackAllocator::release() noexcept {
    if (ownsBuffer_) std::free(buffer_);
    buffer_ = nullptr;
    capacity_ = 0;
    offset_ = 0;
    ownsBuffer_ = false;
}

StackAllocator::StackAllocator(StackAllocator&& other) noexcept
    : buffer_(other.buffer_),
      capacity_(other.capacity_),
      offset_(other.offset_),
      ownsBuffer_(other.ownsBuffer_) {
    other.buffer_ = nullptr;
    other.capacity_ = 0;
    other.offset_ = 0;
    other.ownsBuffer_ = false;
}

StackAllocator& StackAllocator::operator=(StackAllocator&& other) noexcept {
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

void* StackAllocator::allocate(usize size, usize alignment) noexcept {
    ZUKI_ASSERT(isPowerOfTwo(alignment));
    if (buffer_ == nullptr || size == 0) return nullptr;

    const uptr base = reinterpret_cast<uptr>(buffer_);
    const uptr aligned = alignUp(base + offset_, alignment);
    const usize newOffset = (aligned - base) + size;
    if (newOffset > capacity_) return nullptr;

    offset_ = newOffset;
    return reinterpret_cast<void*>(aligned);
}

}  // namespace zuki::memory
