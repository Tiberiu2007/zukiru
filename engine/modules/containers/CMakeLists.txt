# containers — Layer 0. Engine-tuned data structures: sparse set, slot map,
# ring buffer. Header-only. Depends on core (types/assert) and memory (Handle,
# for the slot map's generational handles). Namespace zuki::containers.
add_zuki_module(containers PUBLIC_DEPS core memory)
