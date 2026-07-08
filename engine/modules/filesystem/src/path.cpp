#include <zuki/filesystem/path.hpp>

#include <zuki/core/string_utils.hpp>

#include <string>
#include <vector>

namespace zuki::filesystem::path {

std::string normalize(std::string_view p) {
    std::vector<std::string> segments;
    for (std::string& segment : strings::split(p, '/')) {
        if (segment.empty() || segment == ".") continue;
        if (segment == "..") {
            if (!segments.empty()) segments.pop_back();  // clamp at root
            continue;
        }
        segments.push_back(std::move(segment));
    }
    return "/" + strings::join(segments, "/");
}

std::string join(std::string_view base, std::string_view tail) {
    std::string combined{base};
    combined.push_back('/');
    combined.append(tail);
    return normalize(combined);
}

std::string_view filename(std::string_view p) noexcept {
    const auto pos = p.rfind('/');
    return pos == std::string_view::npos ? p : p.substr(pos + 1);
}

std::string_view extension(std::string_view p) noexcept {
    const std::string_view name = filename(p);
    if (name == "." || name == "..") return {};
    const auto pos = name.rfind('.');
    if (pos == std::string_view::npos || pos == 0) return {};  // none / dotfile
    return name.substr(pos);
}

std::string_view stem(std::string_view p) noexcept {
    const std::string_view name = filename(p);
    const std::string_view ext = extension(p);
    return name.substr(0, name.size() - ext.size());
}

std::string_view parentPath(std::string_view p) noexcept {
    const auto pos = p.rfind('/');
    if (pos == std::string_view::npos) return {};
    if (pos == 0) return p.substr(0, 1);  // parent of "/a" is "/"
    return p.substr(0, pos);
}

bool isAbsolute(std::string_view p) noexcept {
    return !p.empty() && p.front() == '/';
}

}  // namespace zuki::filesystem::path
