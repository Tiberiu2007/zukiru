# ecs — Layer 2 (core subsystems). The engine core: an archetype-based entity /
# component / system World. Components live in struct-of-arrays columns grouped by
# archetype (unique component set), so queries are tight contiguous loops and
# structural changes move an entity between archetypes. Depends on core.
# Namespace zukiru::ecs.
add_zukiru_module(ecs PUBLIC_DEPS core)
