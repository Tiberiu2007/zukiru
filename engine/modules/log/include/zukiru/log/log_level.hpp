// Severity levels for the logging system.
#pragma once

#include <zukiru/core/types.hpp>

#include <optional>
#include <string_view>

namespace zukiru::log {

enum class LogLevel : u8 {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,  // threshold-only: disables a channel/logger; never a record's own level
};

// Fixed-width (5-char) upper-case label, handy for aligned output.
[[nodiscard]] constexpr std::string_view levelName(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO ";
        case LogLevel::Warn: return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRIT ";
        case LogLevel::Off: return "OFF  ";
    }
    return "?????";
}

// ANSI SGR color for a level (empty for none). Pair with `ansiReset()`.
[[nodiscard]] constexpr std::string_view ansiColor(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "\x1b[90m";        // bright black / grey
        case LogLevel::Debug: return "\x1b[36m";        // cyan
        case LogLevel::Info: return "\x1b[32m";         // green
        case LogLevel::Warn: return "\x1b[33m";         // yellow
        case LogLevel::Error: return "\x1b[31m";        // red
        case LogLevel::Critical: return "\x1b[1;31m";   // bold red
        case LogLevel::Off: return "";
    }
    return "";
}
[[nodiscard]] constexpr std::string_view ansiReset() noexcept {
    return "\x1b[0m";
}

// Parse a level name (case-insensitive, accepts trimmed labels like "info").
[[nodiscard]] std::optional<LogLevel> parseLevel(std::string_view text) noexcept;

}  // namespace zukiru::log
