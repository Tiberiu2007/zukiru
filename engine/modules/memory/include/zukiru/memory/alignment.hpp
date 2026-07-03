// Alignment math for allocators. All alignments must be powers of two.
#pragma once

#include <zukiru/core/types.hpp>

#include <cstddef>

namespace zukiru::memory {

// The alignment that satisfies any scalar type (what malloc/new guarantee).
inline constexpr usize kDefaultAlignment = alignof(std::max_align_t);

[[nodiscard]] constexpr bool isPowerOfTwo(usize x) noexcept {
    return x != 0 && (x & (x - 1)) == 0;
}

// Round `value` up to the next multiple of `alignment` (a power of two).
[[nodiscard]] constexpr usize alignUp(usize value, usize alignment) noexcept {
    return (value + (alignment - 1)) & ~(alignment - 1);
}

// Round `value` down to the previous multiple of `alignment`.
[[nodiscard]] constexpr usize alignDown(usize value, usize alignment) noexcept {
    return value & ~(alignment - 1);
}

[[nodiscard]] constexpr bool isAligned(usize value, usize alignment) noexcept {
    return (value & (alignment - 1)) == 0;
}

// Round a pointer up to the requested alignment.
[[nodiscard]] inline void* alignPointer(void* p, usize alignment) noexcept {
    const uptr aligned = alignUp(reinterpret_cast<uptr>(p), alignment);
    return reinterpret_cast<void*>(aligned);
}

}  // namespace zukiru::memory
