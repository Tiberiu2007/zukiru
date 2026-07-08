# core — Layer 0 foundation. The one module everyone depends on; it has no
# dependencies of its own. See docs/adr/0002-core-root-namespace.md for why its
# API lives in the root `zuki` namespace.
add_zuki_module(core)
