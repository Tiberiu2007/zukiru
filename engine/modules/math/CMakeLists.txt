# math — Layer 0. Header-only linear algebra & geometry. Depends only on core
# (for the fixed-width scalar aliases). API lives in the nested `zuki::math`
# namespace (core is the only root-namespace exception; see ADR 0002).
add_zuki_module(math PUBLIC_DEPS core)
