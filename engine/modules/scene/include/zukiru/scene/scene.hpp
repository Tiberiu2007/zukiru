// Scene — a GameObject/scene-graph layer on top of an ECS World.
//
// A Scene owns a World whose every entity is a "node": it has a LocalTransform
// (relative to its parent) and a WorldTransform (cached, in world space). Nodes
// form a parent/child hierarchy; updateTransforms() propagates local transforms
// down the tree into the world-space cache.
//
//   Scene scene;
//   Entity root  = scene.createNode("root");
//   Entity child = scene.createNode("child", root);
//   scene.setLocalTransform(child, {.position = {1, 0, 0}});
//   scene.updateTransforms();
//   scene.worldTransform(child).position;   // root-space -> world-space
//
// Because a Scene *is* an ECS world, you can attach your own components to any
// node and query them via scene.world().each<...>().
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/ecs/world.hpp>
#include <zukiru/math/transform.hpp>
#include <zukiru/scene/components.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace zukiru::scene {

using ecs::Entity;

class Scene {
public:
    Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // The underlying ECS world — attach/query your own components on nodes here.
    [[nodiscard]] ecs::World& world() noexcept { return world_; }
    [[nodiscard]] const ecs::World& world() const noexcept { return world_; }

    // --- Nodes -----------------------------------------------------------

    // Create a node (identity local transform). Parented under `parent`, or a
    // root if `parent` is invalid.
    Entity createNode(std::string name = {}, Entity parent = Entity::invalid());

    // Destroy `node` and its entire subtree.
    void destroyNode(Entity node);

    [[nodiscard]] bool isValid(Entity node) const { return world_.isAlive(node); }
    [[nodiscard]] usize nodeCount() const noexcept { return world_.entityCount(); }

    // Deep-copy `source` and its subtree into a new node (prefab instancing).
    // The clone is parented under `parent` (or a root if invalid). Returns the
    // new subtree root.
    Entity clone(Entity source, Entity parent = Entity::invalid());

    // --- Hierarchy -------------------------------------------------------

    // Re-parent `node` under `newParent` (invalid ⇒ make it a root). Returns
    // false for an invalid node or a move that would create a cycle. Keeps the
    // node's *local* transform (its world position changes accordingly).
    bool setParent(Entity node, Entity newParent);

    [[nodiscard]] Entity parentOf(Entity node) const;
    [[nodiscard]] const std::vector<Entity>& childrenOf(Entity node) const;
    [[nodiscard]] const std::vector<Entity>& roots() const noexcept { return roots_; }

    // --- Transforms ------------------------------------------------------

    [[nodiscard]] const math::Transform& localTransform(Entity node) const;
    void setLocalTransform(Entity node, const math::Transform& transform);

    // World-space transform. Recomputes the hierarchy first if anything changed.
    [[nodiscard]] const math::Transform& worldTransform(Entity node);

    // Recompute every node's WorldTransform from the root local transforms down.
    // A no-op if nothing changed since the last call.
    void updateTransforms();

    // --- Names -----------------------------------------------------------

    [[nodiscard]] std::string_view name(Entity node) const;
    void setName(Entity node, std::string name);

private:
    void attach(Entity child, Entity parent);
    void detachFromCurrentParent(Entity node);
    void eraseFromRoots(Entity node);
    void destroySubtree(Entity node);
    void updateSubtree(Entity node, const math::Transform& parentWorld);
    [[nodiscard]] bool isAncestor(Entity ancestor, Entity node) const;

    ecs::World world_;
    std::vector<Entity> roots_;
    bool transformsDirty_ = false;
};

}  // namespace zukiru::scene
