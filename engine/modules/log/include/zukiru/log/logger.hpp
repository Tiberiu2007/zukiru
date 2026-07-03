// Logger — routes records to sinks, with a global severity threshold plus
// optional per-channel overrides. Thread-safe.
//
// Prefer the macros (they format lazily and stamp source location):
//   ZUKIRU_LOG_INFO("render", "loaded {} meshes in {:.1f}ms", count, ms);
//   ZUKIRU_LOG_ERROR("assets", "failed to open {}", path);
#pragma once

#include <zukiru/core/assert.hpp>  // ZUKIRU_SOURCE_LOCATION
#include <zukiru/core/types.hpp>
#include <zukiru/log/log_level.hpp>
#include <zukiru/log/log_record.hpp>
#include <zukiru/log/sink.hpp>

#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace zukiru::log {

class Logger {
public:
    // --- Sink management -------------------------------------------------
    void addSink(std::shared_ptr<Sink> sink);
    void clearSinks();
    [[nodiscard]] usize sinkCount() const;

    // --- Thresholds ------------------------------------------------------
    void setLevel(LogLevel level);
    [[nodiscard]] LogLevel level() const;
    void setChannelLevel(std::string_view channel, LogLevel level);
    void clearChannelLevel(std::string_view channel);

    // Would a record at `level` on `channel` be emitted?
    [[nodiscard]] bool isEnabled(LogLevel level, std::string_view channel) const;

    // --- Emit ------------------------------------------------------------
    // Dispatches unconditionally to all sinks (the macros do the filtering).
    void log(LogLevel level, std::string_view channel, std::string message,
             SourceLocation location);

    void flush();

private:
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    LogLevel level_ = LogLevel::Info;
    std::map<std::string, LogLevel, std::less<>> channelLevels_;
};

// Process-wide logger. Lazily initialized with a single stderr ConsoleSink.
[[nodiscard]] Logger& defaultLogger();

// Levels below this are removed at compile time (their macros expand to nothing).
#if defined(ZUKIRU_DEBUG) && ZUKIRU_DEBUG
inline constexpr LogLevel kCompiledMinLevel = LogLevel::Trace;
#else
inline constexpr LogLevel kCompiledMinLevel = LogLevel::Info;
#endif

}  // namespace zukiru::log

// --- Macros ---------------------------------------------------------------
// The format arguments are only evaluated when the level passes both the
// compile-time floor and the runtime threshold.
#define ZUKIRU_LOG(channel, lvl, ...)                                                      \
    do {                                                                                   \
        if constexpr ((lvl) >= ::zukiru::log::kCompiledMinLevel) {                         \
            ::zukiru::log::Logger& zukiruLogger_ = ::zukiru::log::defaultLogger();         \
            if (zukiruLogger_.isEnabled((lvl), (channel))) {                               \
                zukiruLogger_.log((lvl), (channel), ::std::format(__VA_ARGS__),            \
                                  ZUKIRU_SOURCE_LOCATION);                                 \
            }                                                                              \
        }                                                                                  \
    } while (false)

#define ZUKIRU_LOG_TRACE(channel, ...) ZUKIRU_LOG(channel, ::zukiru::log::LogLevel::Trace, __VA_ARGS__)
#define ZUKIRU_LOG_DEBUG(channel, ...) ZUKIRU_LOG(channel, ::zukiru::log::LogLevel::Debug, __VA_ARGS__)
#define ZUKIRU_LOG_INFO(channel, ...) ZUKIRU_LOG(channel, ::zukiru::log::LogLevel::Info, __VA_ARGS__)
#define ZUKIRU_LOG_WARN(channel, ...) ZUKIRU_LOG(channel, ::zukiru::log::LogLevel::Warn, __VA_ARGS__)
#define ZUKIRU_LOG_ERROR(channel, ...) ZUKIRU_LOG(channel, ::zukiru::log::LogLevel::Error, __VA_ARGS__)
#define ZUKIRU_LOG_CRITICAL(channel, ...) \
    ZUKIRU_LOG(channel, ::zukiru::log::LogLevel::Critical, __VA_ARGS__)
