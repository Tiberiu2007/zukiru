// StackAllocator — a linear allocator with LIFO rollback. Like ArenaAllocator,
// but you can capture a Marker and later freeToMarker() to release everything
// allocated after it, in stack (last-in-first-out) order. Great for nested
// scopes where lifetimes nest cleanly.
//
// Like the arena, it returns raw storage and runs no destructors.
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/memory/alignment.hpp>

namespace zukiru::memory {

class StackAllocator {
public:
    // A saved position in the stack. Opaque; only valid for its allocator.
    using Marker = usize;

    StackAllocator() = default;
    explicit StackAllocator(usize capacityBytes);
    StackAllocator(void* buffer, usize capacityBytes) noexcept;
    ~StackAllocator();

    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;
    StackAllocator(StackAllocator&& other) noexcept;
    StackAllocator& operator=(StackAllocator&& other) noexcept;

    [[nodiscard]] void* allocate(usize size, usize alignment = kDefaultAlignment) noexcept;

    template <class T>
    [[nodiscard]] T* allocate(usize count = 1) noexcept {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }

    // Capture the current top of the stack.
    [[nodiscard]] Marker mark() const noexcept { return offset_; }

    // Roll back to a previously captured marker (no-op if it's ahead of us).
    void freeToMarker(Marker marker) noexcept {
        if (marker <= offset_) offset_ = marker;
    }

    // Reclaim everything.
    void reset() noexcept { offset_ = 0; }

    [[nodiscard]] usize used() const noexcept { return offset_; }
    [[nodiscard]] usize capacity() const noexcept { return capacity_; }
    [[nodiscard]] usize remaining() const noexcept { return capacity_ - offset_; }

private:
    void release() noexcept;

    byte* buffer_ = nullptr;
    usize capacity_ = 0;
    usize offset_ = 0;
    bool ownsBuffer_ = false;
};

}  // namespace zukiru::memory
