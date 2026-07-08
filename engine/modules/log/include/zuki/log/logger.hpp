// Logger — routes records to sinks, with a global severity threshold plus
// optional per-channel overrides. Thread-safe.
//
// Prefer the macros (they format lazily and stamp source location):
//   ZUKI_LOG_INFO("render", "loaded {} meshes in {:.1f}ms", count, ms);
//   ZUKI_LOG_ERROR("assets", "failed to open {}", path);
#pragma once

#include <zuki/core/assert.hpp>  // ZUKI_SOURCE_LOCATION
#include <zuki/core/types.hpp>
#include <zuki/log/log_level.hpp>
#include <zuki/log/log_record.hpp>
#include <zuki/log/sink.hpp>

#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace zuki::log {

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
#if defined(ZUKI_DEBUG) && ZUKI_DEBUG
inline constexpr LogLevel kCompiledMinLevel = LogLevel::Trace;
#else
inline constexpr LogLevel kCompiledMinLevel = LogLevel::Info;
#endif

}  // namespace zuki::log

// --- Macros ---------------------------------------------------------------
// The format arguments are only evaluated when the level passes both the
// compile-time floor and the runtime threshold.
#define ZUKI_LOG(channel, lvl, ...)                                                      \
    do {                                                                                   \
        if constexpr ((lvl) >= ::zuki::log::kCompiledMinLevel) {                         \
            ::zuki::log::Logger& zukiLogger_ = ::zuki::log::defaultLogger();         \
            if (zukiLogger_.isEnabled((lvl), (channel))) {                               \
                zukiLogger_.log((lvl), (channel), ::std::format(__VA_ARGS__),            \
                                  ZUKI_SOURCE_LOCATION);                                 \
            }                                                                              \
        }                                                                                  \
    } while (false)

#define ZUKI_LOG_TRACE(channel, ...) ZUKI_LOG(channel, ::zuki::log::LogLevel::Trace, __VA_ARGS__)
#define ZUKI_LOG_DEBUG(channel, ...) ZUKI_LOG(channel, ::zuki::log::LogLevel::Debug, __VA_ARGS__)
#define ZUKI_LOG_INFO(channel, ...) ZUKI_LOG(channel, ::zuki::log::LogLevel::Info, __VA_ARGS__)
#define ZUKI_LOG_WARN(channel, ...) ZUKI_LOG(channel, ::zuki::log::LogLevel::Warn, __VA_ARGS__)
#define ZUKI_LOG_ERROR(channel, ...) ZUKI_LOG(channel, ::zuki::log::LogLevel::Error, __VA_ARGS__)
#define ZUKI_LOG_CRITICAL(channel, ...) \
    ZUKI_LOG(channel, ::zuki::log::LogLevel::Critical, __VA_ARGS__)
