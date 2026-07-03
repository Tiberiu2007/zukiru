// Handle<Tag> — a compact, type-safe reference to a slot in some pool/array.
//
// A handle is an index plus a generation counter. When a slot is reused, its
// generation is bumped, so a stale handle (holding the old generation) can be
// detected as invalid — this catches use-after-free of pooled resources without
// raw pointers. `Tag` is a phantom type so handles to different resource kinds
// (e.g. Handle<MeshTag> vs Handle<TextureTag>) are distinct, non-interchangeable
// types even though they share a layout.
#pragma once

#include <zukiru/core/types.hpp>

namespace zukiru::memory {

template <class Tag>
struct Handle {
    static constexpr u32 kInvalidIndex = 0xFFFF'FFFFu;

    u32 index = kInvalidIndex;
    u32 generation = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept { return index != kInvalidIndex; }

    [[nodiscard]] static constexpr Handle invalid() noexcept { return {}; }

    friend constexpr bool operator==(Handle, Handle) = default;
};

static_assert(sizeof(Handle<struct AnyTag>) == 8, "Handle should be a tight 8 bytes");

}  // namespace zukiru::memory
