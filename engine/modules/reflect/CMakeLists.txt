# reflect — Layer 1 (services). Runtime type registry: describe struct types and
# their fields with type-erased accessors, then walk them generically (powers
# serialization and the editor inspector). Header-only. Depends on core.
# Namespace zuki::reflect.
add_zuki_module(reflect PUBLIC_DEPS core)
