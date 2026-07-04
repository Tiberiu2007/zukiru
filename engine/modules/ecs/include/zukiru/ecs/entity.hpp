// Entity — a lightweight generational id (index + generation).
//
// An entity is a handle, not an object: it names a row somewhere in the World's
// archetype storage. The generation lets a stale handle (to an entity that was
// destroyed and whose index was recycled) be detected as no longer alive.
#pragma once

#include <zukiru/core/types.hpp>

#include <functional>

namespace zukiru::ecs {

struct Entity {
    static constexpr u32 kInvalidIndex = 0xFFFF'FFFFu;

    u32 index = kInvalidIndex;
    u32 generation = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept { return index != kInvalidIndex; }
    [[nodiscard]] static constexpr Entity invalid() noexcept { return {}; }

    friend constexpr bool operator==(Entity, Entity) noexcept = default;
};

}  // namespace zukiru::ecs

// Lets Entity be used as a key in unordered containers.
template <>
struct std::hash<zukiru::ecs::Entity> {
    [[nodiscard]] std::size_t operator()(zukiru::ecs::Entity e) const noexcept {
        return (static_cast<std::size_t>(e.generation) << 32) ^ static_cast<std::size_t>(e.index);
    }
};
