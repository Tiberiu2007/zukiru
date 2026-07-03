# platform

**Layer 0 — foundation.** The OS abstraction layer: timing, threads, dynamic
libraries, file I/O, and the windowing surface. Depends only on
[`core`](../core). Namespace `zukiru::platform`.

Everything OS-specific that the rest of the engine needs should funnel through
here, so higher layers stay portable.

## What's here

| Header | Provides |
|--------|----------|
| `<zukiru/platform/clock.hpp>` | `sleepFor`/`sleepMilliseconds`, high-res `performanceCounter`/`performanceFrequency`, `unixTimeSeconds`/`unixTimeMilliseconds`. |
| `<zukiru/platform/thread.hpp>` | `hardwareConcurrency`, `currentThreadId`, `yieldThread`, `setThreadName`/`threadName`. |
| `<zukiru/platform/dynamic_library.hpp>` | `DynamicLibrary` — load `.so`/`.dll`/`.dylib`, resolve symbols/functions. |
| `<zukiru/platform/file_io.hpp>` | `readFile`/`readFileBinary`/`writeFile` (Result-based), `fileExists`/`fileSize`/`removeFile`. |
| `<zukiru/platform/window.hpp>` | `Window` interface + `WindowConfig` (**backend deferred** — see below). |
| `<zukiru/platform/platform.hpp>` | Umbrella header. |

## Timing

Use `core`'s monotonic `Clock`/`Instant` for gameplay timing. This module adds
the lower-level OS surface: sleeping, a nanosecond `performanceCounter()` for
profiling, and wall-clock time. Measure intervals with the performance counter,
never with wall-clock time (which can jump).

## Dynamic libraries

```cpp
DynamicLibrary lib;
if (lib.load("plugins/renderer_vk" + std::string{DynamicLibrary::nativeExtension()})) {
    auto create = lib.getFunction<Renderer*(*)()>("createRenderer");
}
```
An empty path resolves symbols from the current process.

## File I/O

Small, blocking helpers returning `Result`/`Status` (from `core`). The async,
virtual-filesystem layer with mount points is a separate Layer-1 module
(`filesystem`); this is just the raw primitives.

## Windowing (backend deferred)

`window.hpp` defines the abstract `Window` interface and `WindowConfig` so the
renderer and app loop can be designed against them **today**, but no concrete
backend is wired up yet — `createWindow()` returns an error. Choosing a backend
(GLFW is the leading candidate) needs a third-party dependency and a display,
neither of which is available headlessly. Tracked in
[ADR 0003](../../../docs/adr/0003-windowing-backend.md) and `agents/TODO.md`.

## Platform support

Implemented and tested on Linux. macOS paths are provided for threads/dylib/IO;
Windows paths are written behind `ZUKIRU_OS_WINDOWS` but not yet built/verified
in CI. Thread naming is POSIX-only for now (`setThreadName` returns false
elsewhere).

## Tests

```bash
ctest --preset debug -R '^platform\.'
```

Also exercised under `--preset asan`.

## Dependencies

`core` (Layer 0), plus system libraries `Threads::Threads` and the dynamic
loader (`${CMAKE_DL_LIBS}`). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
