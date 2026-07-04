// World — the ECS container: create/destroy entities, attach/detach components,
// and iterate entities by the components they have.
//
//   struct Position { f32 x, y; };
//   struct Velocity { f32 x, y; };
//
//   World world;
//   Entity e = world.create(Position{0, 0}, Velocity{1, 2});
//   world.each<Position, const Velocity>([](Position& p, const Velocity& v) {
//       p.x += v.x; p.y += v.y;
//   });
//   world.get<Position>(e)->x;   // -> 1
//
// Storage is archetype-based (struct-of-arrays): each unique component set is one
// Archetype, so a query over a component set is a tight loop over contiguous
// columns. Adding/removing a component moves the entity to another archetype.
//
// Not thread-safe. Do not create/destroy entities or add/remove components while
// iterating an each() — that restructures the very storage being walked.
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/ecs/archetype.hpp>
#include <zukiru/ecs/component.hpp>
#include <zukiru/ecs/entity.hpp>

#include <map>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zukiru::ecs {

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // --- Entities --------------------------------------------------------

    // Create an entity with no components.
    [[nodiscard]] Entity create();
    // Create an entity carrying the given components.
    template <class... Cs>
    [[nodiscard]] Entity create(Cs&&... components) {
        const Entity e = create();
        (add<std::decay_t<Cs>>(e, std::forward<Cs>(components)), ...);
        return e;
    }

    // Destroy `entity` and all its components. Returns false if it wasn't alive.
    bool destroy(Entity entity);
    [[nodiscard]] bool isAlive(Entity entity) const;
    [[nodiscard]] usize entityCount() const noexcept { return aliveCount_; }

    // --- Components ------------------------------------------------------

    // Attach a component (or overwrite it if already present). Returns a
    // reference to the stored component.
    template <class T>
    T& add(Entity entity, T component) {
        const auto [ptr, existed] = addComponentRaw(entity, componentInfo<T>());
        T* typed = static_cast<T*>(ptr);
        if (existed) {
            *typed = std::move(component);
        } else {
            ::new (ptr) T(std::move(component));
        }
        return *typed;
    }

    // Remove component T. Returns false if the entity lacked it (or is dead).
    template <class T>
    bool remove(Entity entity) {
        return removeComponent(entity, componentId<T>());
    }

    template <class T>
    [[nodiscard]] bool has(Entity entity) const {
        return hasComponent(entity, componentId<T>());
    }

    // Pointer to the entity's T, or nullptr if it has none (or is dead). The
    // pointer is invalidated by any structural change to this entity/archetype.
    template <class T>
    [[nodiscard]] T* get(Entity entity) {
        return static_cast<T*>(getComponent(entity, componentId<T>()));
    }
    template <class T>
    [[nodiscard]] const T* get(Entity entity) const {
        return static_cast<const T*>(getComponent(entity, componentId<T>()));
    }

    // --- Queries ---------------------------------------------------------

    // Invoke `fn` for every entity that has all of Cs. `fn` may take the
    // components either as `(Cs&...)` or as `(Entity, Cs&...)`. Use `const C` in
    // the type list for read-only access.
    template <class... Cs, class Fn>
    void each(Fn&& fn) {
        static_assert(sizeof...(Cs) >= 1, "each<> needs at least one component type");
        const ComponentId ids[] = {componentId<std::remove_const_t<Cs>>()...};
        for (const std::unique_ptr<detail::Archetype>& arch : archetypes_) {
            if (arch->count() == 0) continue;
            if (!archetypeMatches(*arch, ids, sizeof...(Cs))) continue;

            std::tuple<Cs*...> bases{
                static_cast<Cs*>(arch->column(componentId<std::remove_const_t<Cs>>()))...};
            const usize n = arch->count();
            for (usize row = 0; row < n; ++row) {
                invokeRow(fn, arch->entityAt(row), row, bases, std::index_sequence_for<Cs...>{});
            }
        }
    }

    [[nodiscard]] usize archetypeCount() const noexcept { return archetypes_.size(); }

private:
    struct EntityRecord {
        u32 generation = 0;
        bool alive = false;
        detail::Archetype* archetype = nullptr;
        usize row = 0;
    };

    // Type-erased core (defined in the .cpp).
    detail::Archetype* getOrCreateArchetype(std::vector<const ComponentInfo*> infos);
    std::pair<void*, bool> addComponentRaw(Entity entity, const ComponentInfo& info);
    bool removeComponent(Entity entity, ComponentId id);
    [[nodiscard]] bool hasComponent(Entity entity, ComponentId id) const;
    [[nodiscard]] void* getComponent(Entity entity, ComponentId id) const;
    [[nodiscard]] u32 componentTypeIndex(const ComponentInfo& info);
    [[nodiscard]] bool validate(Entity entity) const;

    static bool archetypeMatches(const detail::Archetype& arch, const ComponentId* ids,
                                 usize count) {
        for (usize i = 0; i < count; ++i) {
            if (!arch.has(ids[i])) return false;
        }
        return true;
    }

    template <class Fn, class Tuple, usize... I>
    static void invokeRow(Fn& fn, Entity e, usize row, Tuple& bases, std::index_sequence<I...>) {
        if constexpr (std::is_invocable_v<Fn&, Entity, decltype(*std::get<I>(bases))...>) {
            fn(e, std::get<I>(bases)[row]...);
        } else {
            fn(std::get<I>(bases)[row]...);
        }
    }

    std::vector<EntityRecord> records_;
    std::vector<u32> freeList_;
    usize aliveCount_ = 0;

    std::vector<std::unique_ptr<detail::Archetype>> archetypes_;
    std::map<std::vector<u32>, detail::Archetype*> archetypeBySignature_;
    detail::Archetype* emptyArchetype_ = nullptr;

    std::unordered_map<ComponentId, u32> componentIndex_;
};

}  // namespace zukiru::ecs
