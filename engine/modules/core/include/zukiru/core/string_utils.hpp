// Small, allocation-conscious string helpers. Views in, owning strings out only
// where a new buffer is unavoidable. ASCII-oriented (no locale/Unicode casing).
#pragma once

#include <zukiru/core/types.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace zuki::strings {

// --- Trimming (return views into the input; no allocation) ---------------
[[nodiscard]] std::string_view trimLeft(std::string_view s) noexcept;
[[nodiscard]] std::string_view trimRight(std::string_view s) noexcept;
[[nodiscard]] std::string_view trim(std::string_view s) noexcept;

// --- Predicates ----------------------------------------------------------
[[nodiscard]] bool contains(std::string_view haystack, std::string_view needle) noexcept;
[[nodiscard]] bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept;

// --- Case (ASCII) --------------------------------------------------------
[[nodiscard]] std::string toLower(std::string_view s);
[[nodiscard]] std::string toUpper(std::string_view s);

// --- Split / join --------------------------------------------------------
// Split on every occurrence of `delimiter`. Empty fields are preserved unless
// `skipEmpty` is set. Returned strings own their data (safe past `s`'s lifetime).
[[nodiscard]] std::vector<std::string> split(std::string_view s, char delimiter,
                                             bool skipEmpty = false);
[[nodiscard]] std::vector<std::string> split(std::string_view s, std::string_view delimiter,
                                             bool skipEmpty = false);

// Join `parts` with `separator` between each element.
[[nodiscard]] std::string join(const std::vector<std::string>& parts, std::string_view separator);
[[nodiscard]] std::string join(const std::vector<std::string_view>& parts,
                               std::string_view separator);

// --- Replace -------------------------------------------------------------
// Replace every non-overlapping occurrence of `from` with `to`. A `from` of ""
// returns the input unchanged.
[[nodiscard]] std::string replaceAll(std::string_view s, std::string_view from,
                                     std::string_view to);

}  // namespace zuki::strings
