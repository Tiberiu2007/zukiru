# filesystem

**Layer 1 — services.** A virtual file system (VFS): real host directories are
mounted under virtual prefixes, and virtual paths resolve to real paths by
longest-matching mount. Public API depends on [`core`](../core); the
implementation uses [`platform`](../platform) for the actual file I/O. Namespace
`zuki::filesystem`.

## Why a VFS

Game code shouldn't hard-code where assets live. Mount points let the same
`"/assets/textures/hero.png"` resolve to a dev folder, a packed archive, or a
per-user directory, decided at startup. Because virtual paths are normalized and
`..` is clamped at the root, a path can **never escape its mount** — no directory
traversal attacks.

## Virtual paths (`path::`)

Always `/`-separated and treated as absolute (rooted at the VFS root),
independent of the host OS.

```cpp
using namespace zuki::filesystem;
path::normalize("assets/../textures/./hero.png"); // "/textures/hero.png"
path::normalize("/../../etc/passwd");             // "/etc/passwd"  (clamped)
path::extension("/a/hero.png");                    // ".png"
path::stem("/a/hero.png");                          // "hero"
path::parentPath("/a/b/c");                         // "/a/b"
```

## Mounting & access

```cpp
FileSystem vfs;
vfs.mount("/assets", "/opt/game/assets");           // read-only
vfs.mount("/user",   userDir, /*writable=*/true);   // writable

if (auto text = vfs.readFile("/assets/config.ini"))
    parse(text.value());

vfs.writeFile("/user/saves/slot1.sav", data);       // creates parent dirs
```

| Method | Notes |
|--------|-------|
| `mount(prefix, dir[, writable])` | False if `dir` isn't an existing directory; re-mounting a prefix replaces it. |
| `resolve(vpath)` | Virtual → real host path (`std::optional`), longest-mount-wins. |
| `readFile` / `readFileBinary` | `Result<...>`; error if the path resolves to no mount. |
| `writeFile(vpath, data[, append])` | Only through a `writable` mount; creates parent dirs. |
| `exists` / `fileSize` | Convenience queries. |

Overlapping mounts are resolved by **longest prefix**, so mounting `/assets` and
then `/assets/textures` lets the latter override just that subtree.

## Scope

This is the synchronous, real-directory backend. Async loading, hot-reload, and
packed-archive (`.pak`) mounts layer on top later (`assets` module + the
`pak_builder` tool); the `Mount` abstraction is where archive backends will plug
in.

## Tests

```bash
ctest --preset debug -R '^filesystem\.'
```

Also exercised under `--preset asan`.

## Dependencies

`core` (public), `platform` (private). Layer 1. See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
