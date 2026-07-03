// A small, in-memory string key/value configuration store with typed accessors
// and a minimal `key = value` text format (with `#` / `;` line comments).
//
// Intended for engine/subsystem settings, not a full data-interchange format.
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace zuki {

class Config {
public:
    Config() = default;

    // --- Presence / raw access ------------------------------------------
    [[nodiscard]] bool has(std::string_view key) const;
    [[nodiscard]] usize size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    void clear() noexcept { entries_.clear(); }
    bool remove(std::string_view key);

    // Raw string value if present.
    [[nodiscard]] std::optional<std::string_view> getString(std::string_view key) const;

    // --- Typed getters (std::nullopt if missing or unparsable) -----------
    [[nodiscard]] std::optional<i64> getInt(std::string_view key) const;
    [[nodiscard]] std::optional<f64> getFloat(std::string_view key) const;
    [[nodiscard]] std::optional<bool> getBool(std::string_view key) const;

    // --- Typed getters with a fallback default ---------------------------
    [[nodiscard]] std::string getStringOr(std::string_view key, std::string_view fallback) const;
    [[nodiscard]] i64 getIntOr(std::string_view key, i64 fallback) const;
    [[nodiscard]] f64 getFloatOr(std::string_view key, f64 fallback) const;
    [[nodiscard]] bool getBoolOr(std::string_view key, bool fallback) const;

    // --- Setters ---------------------------------------------------------
    void setString(std::string_view key, std::string_view value);
    void setInt(std::string_view key, i64 value);
    void setFloat(std::string_view key, f64 value);
    void setBool(std::string_view key, bool value);

    // --- Text (de)serialization -----------------------------------------
    // Parse `key = value` lines. Blank lines and lines starting with '#' or ';'
    // (after trimming) are ignored. Returns an Error on a malformed line.
    [[nodiscard]] Status loadFromString(std::string_view text);

    // Serialize back to the same format (keys emitted in sorted order).
    [[nodiscard]] std::string toString() const;

private:
    // std::map keeps deterministic (sorted) output and supports heterogeneous
    // lookup by string_view (C++14 transparent comparator).
    std::map<std::string, std::string, std::less<>> entries_;
};

}  // namespace zuki
