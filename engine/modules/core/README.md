# core

**Layer 0 — foundation.** The one module every other module depends on. `core`
has **no dependencies** of its own.

> **Namespace note:** unlike every other module, `core`'s public API lives in the
> **root `zukiru` namespace** (e.g. `zukiru::i32`, `zukiru::Result`), not `zukiru::core`,
> because it is the shared vocabulary of the whole engine. This is a deliberate,
> documented exception — see
> [ADR 0002](../../../docs/adr/0002-core-root-namespace.md). Please don't "fix"
> it into `zukiru::core`. String and time helpers use the sub-namespaces
> `zukiru::strings` / `zukiru` respectively.

## What's here

| Header | Provides |
|--------|----------|
| `<zukiru/core/types.hpp>` | Fixed-width aliases (`i8`…`u64`, `f32`/`f64`), `usize`/`isize`/`uptr`, `byte`, `Unit`, and `_uz`/`_u32`/`_u64` literals. |
| `<zukiru/core/assert.hpp>` | `ZUKIRU_ASSERT` (debug-only), `ZUKIRU_ENSURE` (always on), `ZUKIRU_ASSERTF`/`ZUKIRU_PANICF` (formatted), `ZUKIRU_UNREACHABLE`, and a swappable `AssertHandler`. |
| `<zukiru/core/result.hpp>` | `Result<T, E=Error>` with `Ok(...)`/`Err(...)`, `map`/`mapErr`/`valueOr`; `Status` (= `Result<void>`); the `Error` type. |
| `<zukiru/core/string_utils.hpp>` | `zukiru::strings`: `trim`, `split`, `join`, `replaceAll`, `toLower`/`toUpper`, `contains`, `equalsIgnoreCase`. |
| `<zukiru/core/time.hpp>` | `Duration`, monotonic `Instant`/`Clock`, `Stopwatch`. |
| `<zukiru/core/config.hpp>` | `Config`: in-memory key/value store with typed getters and a `key = value` text format. |
| `<zukiru/core/core.hpp>` | Umbrella header (convenience; prefer specific includes in your own headers). |

## Examples

```cpp
#include <zukiru/core/result.hpp>

zukiru::Result<Texture> loadTexture(std::string_view path) {
    if (!exists(path)) return zukiru::Err(zukiru::Error{"not found", 404});
    return zukiru::Ok(decode(path));
}

auto r = loadTexture("hero.png");
if (r) use(r.value());
else   log(r.error().message);
```

```cpp
#include <zukiru/core/assert.hpp>

void setVolume(float v) {
    ZUKIRU_ENSURE_MSG(v >= 0.0f && v <= 1.0f, "volume out of [0,1]");  // active in release too
    // ...
}
```

## Error handling policy

The engine is built **without exceptions** for control flow: fallible operations
return `Result`/`Status`. Exceptions are reserved for truly exceptional,
unrecoverable situations. Assertions fail through a swappable `AssertHandler`
(default: print to stderr + `abort`); tests install a throwing handler to observe
failures without terminating.

## Tests

Unit tests live in `tests/` and are wired automatically by
`add_zukiru_module()`. Run them with:

```bash
ctest --preset debug -R '^core\.'
```

## Dependencies

None (Layer 0). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
