#include <zukiru/scene/scene.hpp>

#include <zukiru/core/assert.hpp>

#include <algorithm>
#include <utility>

namespace zukiru::scene {
namespace {

const std::vector<Entity> kNoChildren;

// Compose two TRS transforms: world = parent ∘ child. Assumes no shear (scale is
// combined component-wise), which holds for hierarchies of TRS nodes.
[[nodiscard]] math::Transform combine(const math::Transform& parent,
                                      const math::Transform& child) {
    math::Transform out;
    out.scale = parent.scale * child.scale;
    out.rotation = parent.rotation * child.rotation;
    out.position = parent.position + parent.rotation.rotate(parent.scale * child.position);
    return out;
}

}  // namespace

Scene::Scene() = default;

Entity Scene::createNode(std::string name, Entity parent) {
    const Entity node = world_.create();
    world_.add(node, LocalTransform{});
    world_.add(node, WorldTransform{});
    if (!name.empty()) {
        world_.add(node, Name{std::move(name)});
    }

    if (parent.isValid()) {
        ZUKIRU_ENSURE_MSG(isValid(parent), "createNode: parent is not a live node");
        attach(node, parent);
    } else {
        roots_.push_back(node);
    }
    transformsDirty_ = true;
    return node;
}

void Scene::attach(Entity child, Entity parent) {
    // Ensure the parent has a child list, then link both ways. Each add() is a
    // structural change, so the Children pointer is fetched *after* they settle.
    if (!world_.has<Children>(parent)) {
        world_.add(parent, Children{});
    }
    world_.add(child, Parent{parent});
    world_.get<Children>(parent)->list.push_back(child);
}

void Scene::eraseFromRoots(Entity node) {
    roots_.erase(std::remove(roots_.begin(), roots_.end(), node), roots_.end());
}

void Scene::detachFromCurrentParent(Entity node) {
    const Entity parent = parentOf(node);
    if (parent.isValid()) {
        if (Children* children = world_.get<Children>(parent)) {
            std::vector<Entity>& list = children->list;
            list.erase(std::remove(list.begin(), list.end(), node), list.end());
        }
    } else {
        eraseFromRoots(node);
    }
}

bool Scene::isAncestor(Entity ancestor, Entity node) const {
    Entity current = parentOf(node);
    while (current.isValid()) {
        if (current == ancestor) return true;
        current = parentOf(current);
    }
    return false;
}

bool Scene::setParent(Entity node, Entity newParent) {
    if (!isValid(node)) return false;
    if (newParent.isValid()) {
        if (!isValid(newParent)) return false;
        if (node == newParent) return false;
        if (isAncestor(node, newParent)) return false;  // would create a cycle
    }

    detachFromCurrentParent(node);

    if (newParent.isValid()) {
        attach(node, newParent);
    } else {
        if (world_.has<Parent>(node)) {
            world_.remove<Parent>(node);
        }
        roots_.push_back(node);
    }
    transformsDirty_ = true;
    return true;
}

Entity Scene::parentOf(Entity node) const {
    const Parent* parent = world_.get<Parent>(node);
    return parent != nullptr ? parent->value : Entity::invalid();
}

const std::vector<Entity>& Scene::childrenOf(Entity node) const {
    const Children* children = world_.get<Children>(node);
    return children != nullptr ? children->list : kNoChildren;
}

void Scene::destroySubtree(Entity node) {
    if (const Children* children = world_.get<Children>(node)) {
        const std::vector<Entity> snapshot = children->list;  // copy: destroy mutates archetypes
        for (const Entity child : snapshot) {
            destroySubtree(child);
        }
    }
    world_.destroy(node);
}

void Scene::destroyNode(Entity node) {
    if (!isValid(node)) return;
    detachFromCurrentParent(node);
    destroySubtree(node);
    transformsDirty_ = true;
}

Entity Scene::clone(Entity source, Entity parent) {
    ZUKIRU_ENSURE_MSG(isValid(source), "clone: source is not a live node");

    const Entity copy = world_.create();
    LocalTransform local;
    local.value = world_.get<LocalTransform>(source)->value;
    world_.add(copy, local);
    world_.add(copy, WorldTransform{});
    if (const Name* sourceName = world_.get<Name>(source)) {
        world_.add(copy, Name{sourceName->value});
    }

    if (parent.isValid()) {
        attach(copy, parent);
    } else {
        roots_.push_back(copy);
    }

    // Clone children (snapshot the list first — cloning restructures archetypes).
    if (const Children* children = world_.get<Children>(source)) {
        const std::vector<Entity> snapshot = children->list;
        for (const Entity child : snapshot) {
            clone(child, copy);
        }
    }
    transformsDirty_ = true;
    return copy;
}

const math::Transform& Scene::localTransform(Entity node) const {
    const LocalTransform* local = world_.get<LocalTransform>(node);
    ZUKIRU_ENSURE_MSG(local != nullptr, "localTransform: not a node");
    return local->value;
}

void Scene::setLocalTransform(Entity node, const math::Transform& transform) {
    LocalTransform* local = world_.get<LocalTransform>(node);
    ZUKIRU_ENSURE_MSG(local != nullptr, "setLocalTransform: not a node");
    local->value = transform;
    transformsDirty_ = true;
}

const math::Transform& Scene::worldTransform(Entity node) {
    updateTransforms();
    const WorldTransform* world = world_.get<WorldTransform>(node);
    ZUKIRU_ENSURE_MSG(world != nullptr, "worldTransform: not a node");
    return world->value;
}

void Scene::updateSubtree(Entity node, const math::Transform& parentWorld) {
    const math::Transform world = combine(parentWorld, world_.get<LocalTransform>(node)->value);
    world_.get<WorldTransform>(node)->value = world;

    if (const Children* children = world_.get<Children>(node)) {
        // The child list won't change during a pure transform pass, so iterating
        // it directly is safe.
        for (const Entity child : children->list) {
            updateSubtree(child, world);
        }
    }
}

void Scene::updateTransforms() {
    if (!transformsDirty_) return;
    const math::Transform identity{};
    for (const Entity root : roots_) {
        updateSubtree(root, identity);
    }
    transformsDirty_ = false;
}

std::string_view Scene::name(Entity node) const {
    const Name* named = world_.get<Name>(node);
    return named != nullptr ? std::string_view{named->value} : std::string_view{};
}

void Scene::setName(Entity node, std::string name) {
    ZUKIRU_ENSURE_MSG(isValid(node), "setName: not a live node");
    world_.add(node, Name{std::move(name)});
}

}  // namespace zukiru::scene
