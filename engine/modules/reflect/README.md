# reflect

**Layer 1 — services.** A runtime type registry: describe struct types and their
fields once, then walk them generically at runtime. This is the shared type
information that powers **serialization** and the **editor inspector** — so what
the editor shows and what ships never drift apart. Header-only, depends only on
[`core`](../core). Namespace `zukiru::reflect`.

## Describing a type

```cpp
#include <zukiru/reflect/reflect.hpp>
using namespace zukiru::reflect;

struct Transform { Vec3 position; Quat rotation; f32 scale; };

Registry registry;
registry.registerType<Transform>("Transform")
    .field("position", &Transform::position)
    .field("rotation", &Transform::rotation)
    .field("scale",    &Transform::scale);
```

Registration is explicit (no macros, no static-init magic) so it stays
deterministic and easy to test. `Registry::global()` is the process-wide instance
for engine use; tests and tools can use local registries.

## Walking a type

```cpp
const TypeInfo* info = registry.find<Transform>();   // or find("Transform"), find(id)
for (const Field& f : info->fields) {
    if (f.is<f32>())  save(f.name, f.get<f32>(&instance));
    // ... dispatch on f.type ...
}

// Read / write fields on any instance:
const Field* scale = info->findField("scale");
scale->set<f32>(&t, 2.0f);
f32 s = scale->get<f32>(&t);
```

Field access is **type-erased via a stored member pointer** — no offset
arithmetic, and the reflected type need not be default-constructible. `Field::is<T>()`
checks a field's type; `raw()` yields a `void*` to the field for generic code.

## Nested types

A field whose type is itself registered can be resolved and recursed into —
exactly what a serializer or inspector needs:

```cpp
const Field* pos = info->findField("position");
const TypeInfo* vecInfo = registry.find(pos->type);   // -> "Vec3"
vecInfo->findField("y")->set<f32>(pos->raw(&t), 9.0f);
```

## Design notes

- **RTTI-free** type identity (address of a per-type static), so it works under
  `-fno-rtti`.
- `TypeInfo` records live on the heap and are keyed by both `TypeId` and name;
  the `Registry` is movable, pointers into it stay stable.
- This is the MVP surface (types + fields + accessors). Enums, methods, and
  attribute metadata (ranges, tooltips, serialize flags) can extend `Field`/`TypeInfo`
  later without breaking callers.

## Tests

```bash
ctest --preset debug -R '^reflect\.'
```

Includes an end-to-end demo that serializes a struct to text purely by walking
its reflected fields. Also exercised under `--preset asan`.

## Dependencies

`core` (Layer 1). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
