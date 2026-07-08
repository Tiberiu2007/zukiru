// AssetId — a stable, hashed identity for an asset derived from its virtual path.
//
// Two loads of the same path resolve to the same AssetId, which is how the
// manager de-duplicates and caches. The id is a 64-bit FNV-1a hash of the path;
// id 0 is reserved as "invalid".
#pragma once

#include <zuki/core/types.hpp>

#include <functional>
#include <string_view>

namespace zuki::assets {

struct AssetId {
    u64 value = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept { return value != 0; }

    friend constexpr bool operator==(AssetId, AssetId) noexcept = default;
};

// FNV-1a over the raw path bytes. Deterministic across runs and platforms. The
// path is used verbatim, so callers should pass a normalized virtual path when
// they need "/a/b" and "/a//b" to collide (the manager normalizes for them).
[[nodiscard]] constexpr AssetId assetIdFromPath(std::string_view path) noexcept {
    constexpr u64 kOffsetBasis = 1469598103934665603ull;
    constexpr u64 kPrime = 1099511628211ull;
    u64 hash = kOffsetBasis;
    for (const char c : path) {
        hash ^= static_cast<u64>(static_cast<u8>(c));
        hash *= kPrime;
    }
    // Never hand out the reserved "invalid" id for a real path.
    return AssetId{hash == 0 ? 1ull : hash};
}

}  // namespace zuki::assets

// Enables AssetId as an unordered_map key.
template <>
struct std::hash<zuki::assets::AssetId> {
    [[nodiscard]] std::size_t operator()(zuki::assets::AssetId id) const noexcept {
        return static_cast<std::size_t>(id.value);
    }
};
