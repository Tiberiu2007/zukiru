# memory — Layer 0. Custom allocators (arena, stack, pool), handles, and
# allocation tracking. Depends on core (types, assertions). Namespace
# zukiru::memory.
add_zukiru_module(memory PUBLIC_DEPS core)
