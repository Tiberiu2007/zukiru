#include <zuki/log/log_level.hpp>

#include <zuki/core/string_utils.hpp>

namespace zuki::log {

std::optional<LogLevel> parseLevel(std::string_view text) noexcept {
    const std::string_view s = strings::trim(text);
    using SV = std::string_view;
    if (strings::equalsIgnoreCase(s, SV{"trace"})) return LogLevel::Trace;
    if (strings::equalsIgnoreCase(s, SV{"debug"})) return LogLevel::Debug;
    if (strings::equalsIgnoreCase(s, SV{"info"})) return LogLevel::Info;
    if (strings::equalsIgnoreCase(s, SV{"warn"}) || strings::equalsIgnoreCase(s, SV{"warning"}))
        return LogLevel::Warn;
    if (strings::equalsIgnoreCase(s, SV{"error"})) return LogLevel::Error;
    if (strings::equalsIgnoreCase(s, SV{"crit"}) || strings::equalsIgnoreCase(s, SV{"critical"}))
        return LogLevel::Critical;
    if (strings::equalsIgnoreCase(s, SV{"off"})) return LogLevel::Off;
    return std::nullopt;
}

}  // namespace zuki::log
