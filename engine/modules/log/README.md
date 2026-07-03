# log

**Layer 0 — foundation.** Structured logging with severity levels, named
channels, and pluggable sinks. Depends only on [`core`](../core). Namespace
`zukiru::log`.

## Concepts

- **Level** — `Trace < Debug < Info < Warn < Error < Critical` (plus `Off` as a
  threshold). See `log_level.hpp`.
- **Channel** — a free-form category string (e.g. `"render"`, `"assets"`). Each
  channel can have its own threshold that overrides the logger's global level.
- **Record** — a `LogRecord`: level, channel, message, source location,
  timestamp, and thread id. This is the structured unit handed to sinks.
- **Sink** — an output destination. Sinks are invoked while the `Logger` holds
  its mutex, so they are already serialized and need no internal locking.
  Built-in: `ConsoleSink`, `FileSink`, `CallbackSink`.
- **Logger** — routes records to its sinks. `defaultLogger()` is the
  process-wide instance (lazily created with a single stderr `ConsoleSink`).

## Usage

```cpp
#include <zukiru/log/log.hpp>
using namespace zukiru::log;

ZUKIRU_LOG_INFO("render", "loaded {} meshes in {:.1f}ms", count, ms);
ZUKIRU_LOG_ERROR("assets", "failed to open {}", path);

// Tune verbosity at runtime:
defaultLogger().setLevel(LogLevel::Warn);              // global floor
defaultLogger().setChannelLevel("render", LogLevel::Trace);  // but be chatty here

// Add outputs:
defaultLogger().addSink(std::make_shared<FileSink>("engine.log"));
```

The `ZUKIRU_LOG_*(channel, fmt, ...)` macros use `std::format` and evaluate their
arguments **only** when the level passes both the runtime threshold and the
compile-time floor (`kCompiledMinLevel`). That floor is `Trace` in debug builds
and `Info` in release, so `Trace`/`Debug` calls cost nothing in shipping builds.

## Custom sinks

Derive from `Sink` and implement `write(const LogRecord&)` (and optionally
`flush()`), or use `CallbackSink` to route records to a `std::function` — handy
for tests, in-editor log panels, or forwarding to another system.

## Line format

```
14:32:07.123 [WARN ] [render] dropped frame (renderer.cpp:214)
```

`ConsoleSink` can wrap the level label in ANSI color; `FileSink` never colorizes.

## Tests

```bash
ctest --preset debug -R '^log\.'
```

## Dependencies

`core` (Layer 0). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
