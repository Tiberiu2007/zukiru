# memory — Layer 0. Custom allocators (arena, stack, pool), handles, and
# allocation tracking. Depends on core (types, assertions). Namespace
# zuki::memory.
add_zuki_module(memory PUBLIC_DEPS core)
