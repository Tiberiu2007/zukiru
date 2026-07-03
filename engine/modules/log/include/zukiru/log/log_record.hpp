// One structured log entry, handed to sinks.
#pragma once

#include <zukiru/core/assert.hpp>  // zukiru::SourceLocation
#include <zukiru/core/types.hpp>
#include <zukiru/log/log_level.hpp>

#include <chrono>
#include <string_view>

namespace zukiru::log {

// A single log event. `channel` and `message` are non-owning views that are
// valid for the duration of the synchronous sink dispatch only — a sink that
// defers work must copy them.
struct LogRecord {
    LogLevel level = LogLevel::Info;
    std::string_view channel;
    std::string_view message;
    SourceLocation location;
    std::chrono::system_clock::time_point time;
    u64 threadId = 0;
};

}  // namespace zukiru::log
