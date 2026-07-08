# platform

**Layer 0 — foundation.** The OS abstraction layer: timing, threads, dynamic
libraries, file I/O, and the windowing surface. Depends only on
[`core`](../core). Namespace `zuki::platform`.

Everything OS-specific that the rest of the engine needs should funnel through
here, so higher layers stay portable.

## What's here

| Header | Provides |
|--------|----------|
| `<zuki/platform/clock.hpp>` | `sleepFor`/`sleepMilliseconds`, high-res `performanceCounter`/`performanceFrequency`, `unixTimeSeconds`/`unixTimeMilliseconds`. |
| `<zuki/platform/thread.hpp>` | `hardwareConcurrency`, `currentThreadId`, `yieldThread`, `setThreadName`/`threadName`. |
| `<zuki/platform/dynamic_library.hpp>` | `DynamicLibrary` — load `.so`/`.dll`/`.dylib`, resolve symbols/functions. |
| `<zuki/platform/file_io.hpp>` | `readFile`/`readFileBinary`/`writeFile` (Result-based), `fileExists`/`fileSize`/`removeFile`. |
| `<zuki/platform/window.hpp>` | `Window` interface + `WindowConfig` (**backend deferred** — see below). |
| `<zuki/platform/platform.hpp>` | Umbrella header. |

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

## Windowing

`window.hpp` defines the abstract `Window` + `WindowConfig`, plus backend-neutral
`WindowEvent`s and platform-level input codes (`Key`, `MouseButton`, `KeyMods`).
`createWindow()` picks a **native** backend at runtime — **no GLFW/SDL**:

- **Wayland** (libwayland-client + xdg-shell + xkbcommon) when `WAYLAND_DISPLAY` is set,
- **X11** (Xlib) when `DISPLAY` is set,
- otherwise a clear `Error`.

```cpp
auto win = platform::createWindow({.title = "Zuki", .width = 1280, .height = 720});
while (!win.value()->shouldClose()) {
    win.value()->pollEvents();
    for (const platform::WindowEvent& e : win.value()->events()) { /* ... */ }
}
```

Backends are opt-out via the `ZUKI_WINDOW_X11` / `ZUKI_WINDOW_WAYLAND` CMake
options (default ON when their libraries are found). `nativeHandle()` /
`nativeDisplay()` expose the raw surface + display for a future Vulkan backend.
Feed events into the `input` module with `input/platform_bridge.hpp`. See
[ADR 0005](../../../docs/adr/0005-native-windowing-backends.md).

## Platform support

Implemented and tested on Linux (X11 exercised against a live display; Wayland
builds/links and is validated on a Wayland session). macOS paths exist for
threads/dylib/IO; Windows paths are written behind `ZUKI_OS_WINDOWS` but not yet
built/verified. **Windowing on Win32 (Cocoa) is future work.** Thread naming is
POSIX-only for now (`setThreadName` returns false elsewhere).

## Tests

```bash
ctest --preset debug -R '^platform\.'
```

Also exercised under `--preset asan`.

## Dependencies

`core` (Layer 0), plus system libraries `Threads::Threads` and the dynamic
loader (`${CMAKE_DL_LIBS}`). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
