#include <zukiru/core/config.hpp>

#include <zukiru/core/string_utils.hpp>

#include <charconv>
#include <format>

namespace zuki {

bool Config::has(std::string_view key) const {
    return entries_.find(key) != entries_.end();
}

bool Config::remove(std::string_view key) {
    const auto it = entries_.find(key);
    if (it == entries_.end()) return false;
    entries_.erase(it);
    return true;
}

std::optional<std::string_view> Config::getString(std::string_view key) const {
    const auto it = entries_.find(key);
    if (it == entries_.end()) return std::nullopt;
    return std::string_view{it->second};
}

std::optional<i64> Config::getInt(std::string_view key) const {
    const auto v = getString(key);
    if (!v) return std::nullopt;
    const std::string_view s = strings::trim(*v);
    i64 out = 0;
    const auto* last = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(s.data(), last, out);
    if (ec != std::errc{} || ptr != last) return std::nullopt;
    return out;
}

std::optional<f64> Config::getFloat(std::string_view key) const {
    const auto v = getString(key);
    if (!v) return std::nullopt;
    const std::string_view s = strings::trim(*v);
    f64 out = 0.0;
    const auto* last = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(s.data(), last, out);
    if (ec != std::errc{} || ptr != last) return std::nullopt;
    return out;
}

std::optional<bool> Config::getBool(std::string_view key) const {
    const auto v = getString(key);
    if (!v) return std::nullopt;
    const std::string_view s = strings::trim(*v);
    if (strings::equalsIgnoreCase(s, "true") || s == "1" || strings::equalsIgnoreCase(s, "yes") ||
        strings::equalsIgnoreCase(s, "on")) {
        return true;
    }
    if (strings::equalsIgnoreCase(s, "false") || s == "0" || strings::equalsIgnoreCase(s, "no") ||
        strings::equalsIgnoreCase(s, "off")) {
        return false;
    }
    return std::nullopt;
}

std::string Config::getStringOr(std::string_view key, std::string_view fallback) const {
    const auto v = getString(key);
    return std::string{v ? *v : fallback};
}

i64 Config::getIntOr(std::string_view key, i64 fallback) const {
    return getInt(key).value_or(fallback);
}

f64 Config::getFloatOr(std::string_view key, f64 fallback) const {
    return getFloat(key).value_or(fallback);
}

bool Config::getBoolOr(std::string_view key, bool fallback) const {
    return getBool(key).value_or(fallback);
}

void Config::setString(std::string_view key, std::string_view value) {
    entries_[std::string{key}] = std::string{value};
}

void Config::setInt(std::string_view key, i64 value) {
    entries_[std::string{key}] = std::format("{}", value);
}

void Config::setFloat(std::string_view key, f64 value) {
    entries_[std::string{key}] = std::format("{}", value);
}

void Config::setBool(std::string_view key, bool value) {
    entries_[std::string{key}] = value ? "true" : "false";
}

Status Config::loadFromString(std::string_view text) {
    i32 lineNo = 0;
    for (const std::string& rawLine : strings::split(text, '\n')) {
        ++lineNo;
        const std::string_view line = strings::trim(rawLine);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;

        const usize eq = line.find('=');
        if (eq == std::string_view::npos) {
            return Err(Error{std::format("config: line {}: missing '=' in \"{}\"", lineNo, line)});
        }
        const std::string_view key = strings::trim(line.substr(0, eq));
        const std::string_view value = strings::trim(line.substr(eq + 1));
        if (key.empty()) {
            return Err(Error{std::format("config: line {}: empty key", lineNo)});
        }
        setString(key, value);
    }
    return Ok();
}

std::string Config::toString() const {
    std::string out;
    for (const auto& [key, value] : entries_) {
        out += std::format("{} = {}\n", key, value);
    }
    return out;
}

}  // namespace zuki
