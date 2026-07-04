#include <zukiru/scene/scene.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>

using namespace zukiru;
using namespace zukiru::scene;
using Catch::Matchers::WithinAbs;

namespace {

bool contains(const std::vector<Entity>& v, Entity e) {
    return std::find(v.begin(), v.end(), e) != v.end();
}

}  // namespace

TEST_CASE("create nodes as roots and children", "[scene]") {
    Scene scene;
    REQUIRE(scene.nodeCount() == 0);

    const Entity root = scene.createNode("root");
    const Entity child = scene.createNode("child", root);

    REQUIRE(scene.nodeCount() == 2);
    REQUIRE(scene.isValid(root));
    REQUIRE(scene.name(root) == "root");
    REQUIRE(scene.name(child) == "child");

    REQUIRE(contains(scene.roots(), root));
    REQUIRE_FALSE(contains(scene.roots(), child));
    REQUIRE(scene.parentOf(child) == root);
    REQUIRE(scene.parentOf(root) == Entity::invalid());
    REQUIRE(contains(scene.childrenOf(root), child));
}

TEST_CASE("world transform composes down the hierarchy", "[scene]") {
    Scene scene;
    const Entity root = scene.createNode("root");
    const Entity child = scene.createNode("child", root);

    scene.setLocalTransform(root, {.position = {10.0f, 0.0f, 0.0f}});
    scene.setLocalTransform(child, {.position = {1.0f, 2.0f, 0.0f}});

    const math::Transform& w = scene.worldTransform(child);
    REQUIRE_THAT(w.position.x, WithinAbs(11.0f, 1e-5f));
    REQUIRE_THAT(w.position.y, WithinAbs(2.0f, 1e-5f));

    // The root itself is at its local position.
    REQUIRE_THAT(scene.worldTransform(root).position.x, WithinAbs(10.0f, 1e-5f));
}

TEST_CASE("parent scale and rotation affect children", "[scene]") {
    Scene scene;
    const Entity root = scene.createNode("root");
    const Entity child = scene.createNode("child", root);

    // Parent scaled 2x; child offset by 3 on X.
    scene.setLocalTransform(root, {.position = {0.0f, 0.0f, 0.0f},
                                   .rotation = math::Quat::identity(),
                                   .scale = {2.0f, 2.0f, 2.0f}});
    scene.setLocalTransform(child, {.position = {3.0f, 0.0f, 0.0f}});

    // Child world position = parent.scale * child.position = 6.
    REQUIRE_THAT(scene.worldTransform(child).position.x, WithinAbs(6.0f, 1e-5f));
}

TEST_CASE("reparenting moves a node between parents and roots", "[scene]") {
    Scene scene;
    const Entity a = scene.createNode("a");
    const Entity b = scene.createNode("b");
    const Entity child = scene.createNode("child", a);

    REQUIRE(contains(scene.childrenOf(a), child));

    REQUIRE(scene.setParent(child, b));
    REQUIRE(scene.parentOf(child) == b);
    REQUIRE_FALSE(contains(scene.childrenOf(a), child));
    REQUIRE(contains(scene.childrenOf(b), child));

    // Detach to root.
    REQUIRE(scene.setParent(child, Entity::invalid()));
    REQUIRE(scene.parentOf(child) == Entity::invalid());
    REQUIRE(contains(scene.roots(), child));
    REQUIRE_FALSE(contains(scene.childrenOf(b), child));
}

TEST_CASE("reparenting rejects cycles and self-parenting", "[scene]") {
    Scene scene;
    const Entity root = scene.createNode("root");
    const Entity mid = scene.createNode("mid", root);
    const Entity leaf = scene.createNode("leaf", mid);

    REQUIRE_FALSE(scene.setParent(root, root));   // self
    REQUIRE_FALSE(scene.setParent(root, leaf));   // ancestor under descendant
    REQUIRE_FALSE(scene.setParent(mid, leaf));    // cycle
    REQUIRE(scene.setParent(leaf, root));         // valid: leaf under root
    REQUIRE(scene.parentOf(leaf) == root);
}

TEST_CASE("destroying a node removes its whole subtree", "[scene]") {
    Scene scene;
    const Entity root = scene.createNode("root");
    const Entity mid = scene.createNode("mid", root);
    const Entity leaf = scene.createNode("leaf", mid);
    const Entity other = scene.createNode("other");
    REQUIRE(scene.nodeCount() == 4);

    scene.destroyNode(mid);
    REQUIRE_FALSE(scene.isValid(mid));
    REQUIRE_FALSE(scene.isValid(leaf));  // subtree gone
    REQUIRE(scene.isValid(root));
    REQUIRE(scene.isValid(other));
    REQUIRE_FALSE(contains(scene.childrenOf(root), mid));
    REQUIRE(scene.nodeCount() == 2);
}

TEST_CASE("destroying a root cleans up its subtree and the roots list", "[scene]") {
    Scene scene;
    const Entity root = scene.createNode("root");
    (void)scene.createNode("child", root);

    scene.destroyNode(root);
    REQUIRE(scene.nodeCount() == 0);
    REQUIRE(scene.roots().empty());
    REQUIRE_FALSE(contains(scene.roots(), root));
}

TEST_CASE("clone deep-copies a subtree (prefab instancing)", "[scene]") {
    Scene scene;
    const Entity prefab = scene.createNode("prefab");
    const Entity part = scene.createNode("part", prefab);
    scene.setLocalTransform(prefab, {.position = {5.0f, 0.0f, 0.0f}});
    scene.setLocalTransform(part, {.position = {1.0f, 0.0f, 0.0f}});

    const Entity instance = scene.clone(prefab);
    REQUIRE(instance != prefab);
    REQUIRE(scene.name(instance) == "prefab");
    REQUIRE(scene.nodeCount() == 4);  // prefab+part and instance+its part

    // The clone has its own child, not the original's.
    REQUIRE(scene.childrenOf(instance).size() == 1);
    const Entity clonedPart = scene.childrenOf(instance).front();
    REQUIRE(clonedPart != part);
    REQUIRE(scene.name(clonedPart) == "part");

    // Transforms were copied, so world positions match the original.
    REQUIRE_THAT(scene.worldTransform(clonedPart).position.x,
                 WithinAbs(scene.worldTransform(part).position.x, 1e-5f));

    // Mutating the clone doesn't touch the original.
    scene.setLocalTransform(instance, {.position = {-5.0f, 0.0f, 0.0f}});
    REQUIRE_THAT(scene.worldTransform(prefab).position.x, WithinAbs(5.0f, 1e-5f));
    REQUIRE_THAT(scene.worldTransform(instance).position.x, WithinAbs(-5.0f, 1e-5f));
}

TEST_CASE("clone under a parent nests the instance", "[scene]") {
    Scene scene;
    const Entity holder = scene.createNode("holder");
    const Entity prefab = scene.createNode("prefab");

    const Entity instance = scene.clone(prefab, holder);
    REQUIRE(scene.parentOf(instance) == holder);
    REQUIRE(contains(scene.childrenOf(holder), instance));
    REQUIRE_FALSE(contains(scene.roots(), instance));
}

TEST_CASE("scene world is a usable ECS world", "[scene]") {
    struct Health {
        i32 value;
    };
    Scene scene;
    const Entity e = scene.createNode("unit");
    scene.world().add(e, Health{42});

    int seen = 0;
    scene.world().each<Health>([&](Health& h) {
        REQUIRE(h.value == 42);
        ++seen;
    });
    REQUIRE(seen == 1);
    // Node components coexist with user components.
    REQUIRE(scene.world().has<LocalTransform>(e));
}

TEST_CASE("worldTransform is stable when nothing changes", "[scene]") {
    Scene scene;
    const Entity n = scene.createNode("n");
    scene.setLocalTransform(n, {.position = {7.0f, 0.0f, 0.0f}});
    const f32 first = scene.worldTransform(n).position.x;
    const f32 second = scene.worldTransform(n).position.x;  // no dirtying in between
    REQUIRE(first == second);
    REQUIRE_THAT(second, WithinAbs(7.0f, 1e-5f));
}
