// Component identity and the type-erased operations the storage needs.
//
// Components are plain user structs — no base class. Because the World stores
// them in raw column buffers keyed by type, it needs, per component type, its
// size/alignment plus function pointers to move-construct and destruct one (used
// when growing a column or moving an entity between archetypes).
#pragma once

#include <zuki/core/types.hpp>

#include <new>
#include <utility>

namespace zuki::ecs {

// RTTI-free per-type identity (address of a per-instantiation static), so ecs
// works under -fno-rtti.
using ComponentId = const void*;

struct ComponentInfo {
    ComponentId id;
    usize size;
    usize alignment;
    // Move-construct a T at `dst` from the T at `src` (src stays destructible).
    void (*moveConstruct)(void* dst, void* src);
    // Run T's destructor on the object at `p`.
    void (*destruct)(void* p);
};

// The stable ComponentInfo for T. Requirements on T: move-constructible and
// destructible (components are relocated when archetypes change).
template <class T>
[[nodiscard]] const ComponentInfo& componentInfo() noexcept {
    static_assert(std::is_move_constructible_v<T>, "components must be move-constructible");
    static_assert(std::is_destructible_v<T>, "components must be destructible");
    static const ComponentInfo info{
        /*id=*/[] { static const char marker{}; return &marker; }(),
        /*size=*/sizeof(T),
        /*alignment=*/alignof(T),
        /*moveConstruct=*/
        [](void* dst, void* src) { ::new (dst) T(std::move(*static_cast<T*>(src))); },
        /*destruct=*/[](void* p) { static_cast<T*>(p)->~T(); },
    };
    return info;
}

template <class T>
[[nodiscard]] ComponentId componentId() noexcept {
    return componentInfo<T>().id;
}

}  // namespace zuki::ecs
