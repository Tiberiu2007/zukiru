# scene — Layer 2 (core subsystems). A GameObject/scene-graph layer on top of the
# ECS World: nodes with local/world transforms, a parent/child hierarchy with
# transform propagation, and prefab instancing (subtree clone). Depends on core,
# ecs (owns a World) and math (transform types). Namespace zuki::scene.
add_zuki_module(scene PUBLIC_DEPS core ecs math)
