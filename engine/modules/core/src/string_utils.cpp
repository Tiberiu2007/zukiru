#include <zukiru/core/string_utils.hpp>

#include <cctype>

namespace zukiru::strings {
namespace {

constexpr std::string_view kWhitespace = " \t\n\r\f\v";

char asciiLower(char c) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}
char asciiUpper(char c) noexcept {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

}  // namespace

std::string_view trimLeft(std::string_view s) noexcept {
    const usize pos = s.find_first_not_of(kWhitespace);
    return pos == std::string_view::npos ? std::string_view{} : s.substr(pos);
}

std::string_view trimRight(std::string_view s) noexcept {
    const usize pos = s.find_last_not_of(kWhitespace);
    return pos == std::string_view::npos ? std::string_view{} : s.substr(0, pos + 1);
}

std::string_view trim(std::string_view s) noexcept {
    return trimLeft(trimRight(s));
}

bool contains(std::string_view haystack, std::string_view needle) noexcept {
    return haystack.find(needle) != std::string_view::npos;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (usize i = 0; i < a.size(); ++i) {
        if (asciiLower(a[i]) != asciiLower(b[i])) return false;
    }
    return true;
}

std::string toLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(asciiLower(c));
    return out;
}

std::string toUpper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(asciiUpper(c));
    return out;
}

std::vector<std::string> split(std::string_view s, char delimiter, bool skipEmpty) {
    return split(s, std::string_view{&delimiter, 1}, skipEmpty);
}

std::vector<std::string> split(std::string_view s, std::string_view delimiter, bool skipEmpty) {
    std::vector<std::string> out;
    if (delimiter.empty()) {
        if (!s.empty() || !skipEmpty) out.emplace_back(s);
        return out;
    }
    usize start = 0;
    while (true) {
        const usize pos = s.find(delimiter, start);
        std::string_view field =
            (pos == std::string_view::npos) ? s.substr(start) : s.substr(start, pos - start);
        if (!skipEmpty || !field.empty()) out.emplace_back(field);
        if (pos == std::string_view::npos) break;
        start = pos + delimiter.size();
    }
    return out;
}

namespace {

template <class Parts>
std::string joinImpl(const Parts& parts, std::string_view separator) {
    std::string out;
    usize total = 0;
    for (const auto& p : parts) total += p.size();
    if (!parts.empty()) total += separator.size() * (parts.size() - 1);
    out.reserve(total);

    bool first = true;
    for (const auto& p : parts) {
        if (!first) out.append(separator);
        out.append(p.data(), p.size());
        first = false;
    }
    return out;
}

}  // namespace

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    return joinImpl(parts, separator);
}

std::string join(const std::vector<std::string_view>& parts, std::string_view separator) {
    return joinImpl(parts, separator);
}

std::string replaceAll(std::string_view s, std::string_view from, std::string_view to) {
    if (from.empty()) return std::string{s};
    std::string out;
    out.reserve(s.size());
    usize start = 0;
    while (true) {
        const usize pos = s.find(from, start);
        if (pos == std::string_view::npos) {
            out.append(s.substr(start));
            break;
        }
        out.append(s.substr(start, pos - start));
        out.append(to);
        start = pos + from.size();
    }
    return out;
}

}  // namespace zukiru::strings
