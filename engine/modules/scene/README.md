# scene

**Layer 2 — core subsystems.** A GameObject/scene-graph layer on top of the ECS
`World`: nodes with local/world transforms, a parent/child hierarchy with
transform propagation, and prefab instancing (subtree clone).

Depends on [`core`](../core), [`ecs`](../ecs) (a Scene owns a `World`) and
[`math`](../math) (transform types). Namespace `zuki::scene`.

## Nodes are entities

A Scene owns an ECS `World` in which every entity is a **node** — it carries a
`LocalTransform` (relative to its parent) and a cached `WorldTransform`. Hierarchy
is expressed with `Parent`/`Children` components. Because a Scene *is* a world,
you can attach and query your own components on any node:

```cpp
#include <zuki/scene/scene.hpp>
using namespace zuki::scene;

Scene scene;
Entity root  = scene.createNode("root");
Entity child = scene.createNode("child", root);

scene.world().add(child, Health{100});          // your own component
scene.world().each<Health>([](Health& h) { /* ... */ });
```

## Transforms

Set **local** transforms; read **world** transforms. `worldTransform()`
recomputes the hierarchy on demand (only if something changed since last call):

```cpp
scene.setLocalTransform(root,  {.position = {10, 0, 0}});
scene.setLocalTransform(child, {.position = {1, 2, 0}});

scene.worldTransform(child).position;   // {11, 2, 0} — parent ∘ child
```

World transforms compose full TRS down the tree (parent scale and rotation affect
children). Composition assumes no shear — true for a hierarchy of TRS nodes.
`updateTransforms()` propagates explicitly (e.g. once per frame) if you'd rather
not trigger it lazily.

## Hierarchy

```cpp
scene.setParent(child, otherParent);            // re-parent (keeps local transform)
scene.setParent(child, Entity::invalid());      // detach to a root
scene.parentOf(child);   scene.childrenOf(root);   scene.roots();
scene.destroyNode(root);                         // destroys the whole subtree
```

`setParent` returns `false` and does nothing if the move would create a cycle
(re-parenting a node under its own descendant) or the node is invalid.

## Prefabs

`clone()` deep-copies a node and its subtree into fresh entities — the primitive
for prefab instancing:

```cpp
Entity instance = scene.clone(prefab);            // new root
Entity nested   = scene.clone(prefab, container);  // parented under `container`
```

The clone is fully independent: it has its own child entities and transforms;
editing one doesn't touch the other.

## Scope

MVP: nodes, transform hierarchy + propagation, re-parenting (cycle-safe), subtree
destroy, and clone-based prefab instancing. Deferred (additive): world-preserving
re-parenting, per-subtree dirty tracking (currently a whole-graph recompute when
anything changes), and on-disk (de)serialization — the latter will layer on
`reflect`-driven data serialization rather than a hand-rolled format.

## Tests

```bash
ctest --preset debug -R '^scene\.'
```

11 unit tests: hierarchy wiring, TRS composition (position/scale), cycle-safe
re-parenting, subtree destruction, prefab clone independence, and ECS
interop. Also run under `--preset asan`.

## Dependencies

`core`, `ecs`, `math` (Layer 2). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
