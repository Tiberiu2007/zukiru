# assets

**Layer 1 — services.** The asset manager: register **importers**, then load
resources by virtual path. It gives you an **importer registry**, **async
loading**, **ref-counted typed handles**, **hot-reload**, and **ref-counted
eviction** — the plumbing every other subsystem loads its data through.

Public API depends only on [`core`](../core); the implementation reads bytes via
[`filesystem`](../filesystem) and runs async loads on [`jobs`](../jobs) (both
forward-declared in the header, so including `assets` stays cheap). Namespace
`zukiru::assets`.

## Register importers, then load

```cpp
#include <zukiru/assets/assets.hpp>
using namespace zukiru::assets;

struct Texture { u32 w, h; std::vector<byte> pixels; };

AssetManager assets(vfs, &jobs);   // filesystem required; jobs optional
assets.registerImporter<Texture>({"png", "tga"},
    [](std::span<const byte> bytes) -> Result<std::shared_ptr<Texture>> {
        return Ok(std::make_shared<Texture>(decodeImage(bytes)));
    });

AssetHandle<Texture> hero = assets.load<Texture>("/textures/hero.png");        // blocking
AssetHandle<Texture> bg   = assets.loadAsync<Texture>("/textures/bg.png");     // on the pool
assets.waitForPending();

if (hero.isLoaded()) draw(*hero);
```

An importer turns file bytes into a `T` (or an `Error`). Extensions match with or
without a leading dot, case-insensitively.

## Handles

`AssetHandle<T>` is a cheap, copyable, **ref-counted** reference:

- `get()` / `operator->` — the content, or `nullptr` until the first successful
  load. The pointer is valid while a handle to it lives and it isn't
  reloaded/unloaded.
- `state()` → `Unloaded` / `Loading` / `Loaded` / `Failed`; `isLoaded()`,
  `isFailed()`, `error()`.
- `version()` — increments on each successful (re)load, so you can detect a
  hot-reload by comparing against a cached value.

Repeated `load()`s of the same path (after normalization) share one record.

## Hot-reload

```cpp
assets.reload("/textures/hero.png");   // re-import in place, bump version()
assets.reloadAll();                    // re-import everything cached
```

Reload swaps content **in place**, so every existing handle immediately sees the
new data. A failed reload (e.g. a bad edit) leaves the asset `Failed` but **keeps
the last good content** — you don't lose a working asset to a typo.

## Lifetime

The cache holds each asset until you drop it:

```cpp
assets.unload("/old.png");      // remove from cache; live handles keep their copy
assets.garbageCollect();        // evict every cached asset no handle references
```

`garbageCollect()` only evicts records with no external handle (and never one with
an in-flight async load), giving ref-counted unloading without dangling.

## Threading

`load` / `loadAsync` / `reload` / `find` and async completion are mutex-guarded
and safe to call concurrently; records publish across threads via atomics
(`state` release/acquire, `std::atomic<std::shared_ptr>` content). Verified under
**tsan**. A `T*` from `get()` stays valid while its handle lives and the asset is
not concurrently reloaded/unloaded.

## Scope

MVP: importer-registry + handles + async + hot-reload + GC. Not yet: dependency
graphs between assets, packed-archive backends (layer on `filesystem`), a
file-watcher to trigger reload automatically, or `reflect`-driven generic
(de)serialization of asset data. These extend the API without breaking it.

## Tests

```bash
ctest --preset debug -R '^assets\.'
```

Real temp-dir files through a mounted VFS; async path exercised with a live
`JobSystem`. Also run under `--preset asan` and `--preset tsan`.

## Dependencies

`core` (public), `filesystem` + `jobs` (public — they appear in the constructor).
See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
