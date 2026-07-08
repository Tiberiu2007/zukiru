// ArenaAllocator — a linear "bump" allocator. Hands out ever-advancing slices
// of a single buffer; individual frees are not supported. reset() reclaims the
// whole arena at once. Ideal for per-frame / per-pass scratch memory.
//
// Does NOT run constructors or destructors — it returns raw storage. Objects
// placed here must be trivially destructible or destroyed by the caller before
// reset().
#pragma once

#include <zuki/core/types.hpp>
#include <zuki/memory/alignment.hpp>

namespace zuki::memory {

class ArenaAllocator {
public:
    ArenaAllocator() = default;

    // Own a freshly heap-allocated buffer of `capacityBytes`.
    explicit ArenaAllocator(usize capacityBytes);

    // Borrow an externally owned buffer (non-owning; caller keeps it alive).
    ArenaAllocator(void* buffer, usize capacityBytes) noexcept;

    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&& other) noexcept;
    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

    // Allocate `size` bytes with the given alignment. Returns nullptr if the
    // arena is exhausted (never throws).
    [[nodiscard]] void* allocate(usize size, usize alignment = kDefaultAlignment) noexcept;

    // Typed uninitialized allocation of `count` Ts (no construction).
    template <class T>
    [[nodiscard]] T* allocate(usize count = 1) noexcept {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }

    // Reclaim everything; existing pointers become dangling.
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

}  // namespace zuki::memory
