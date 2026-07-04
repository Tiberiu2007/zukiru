// The components the scene graph layers onto ECS entities.
//
// A "node" is an entity carrying a LocalTransform + WorldTransform; hierarchy is
// expressed with Parent / Children components, and Name is optional metadata.
// These are plain ECS components, so you can also query them directly through
// `Scene::world()`.
#pragma once

#include <zukiru/ecs/entity.hpp>
#include <zukiru/math/transform.hpp>

#include <string>
#include <vector>

namespace zukiru::scene {

// The node's transform relative to its parent (or to the world, for a root).
struct LocalTransform {
    math::Transform value{};
};

// The node's transform in world space — a cache recomputed by
// Scene::updateTransforms() from the local transforms down the hierarchy.
struct WorldTransform {
    math::Transform value{};
};

// Link to the parent node. Absent on root nodes.
struct Parent {
    ecs::Entity value = ecs::Entity::invalid();
};

// The node's direct children, in order. Absent until the node gains a child.
struct Children {
    std::vector<ecs::Entity> list;
};

// Optional human-readable name.
struct Name {
    std::string value;
};

}  // namespace zukiru::scene
