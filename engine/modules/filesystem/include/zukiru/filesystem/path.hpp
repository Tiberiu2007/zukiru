// Virtual-path utilities. VFS paths always use '/' separators and are treated as
// absolute (rooted at the VFS root), independent of the host OS path syntax.
#pragma once

#include <string>
#include <string_view>

namespace zukiru::filesystem::path {

// Canonicalize a virtual path: collapse '.'/'..' (clamped at root), drop empty
// and redundant separators, and force a single leading '/'. E.g.
//   "assets/../textures/./hero.png"  ->  "/textures/hero.png"
//   ".."                             ->  "/"
[[nodiscard]] std::string normalize(std::string_view p);

// Join two virtual paths and normalize the result.
[[nodiscard]] std::string join(std::string_view base, std::string_view tail);

// Final component ("/a/b/c.png" -> "c.png"). View into `p`.
[[nodiscard]] std::string_view filename(std::string_view p) noexcept;

// Extension including the leading dot ("hero.png" -> ".png"); empty if none or
// for dotfiles like ".gitignore". View into `p`.
[[nodiscard]] std::string_view extension(std::string_view p) noexcept;

// Filename without its extension ("hero.png" -> "hero"). View into `p`.
[[nodiscard]] std::string_view stem(std::string_view p) noexcept;

// Everything before the final component ("/a/b/c" -> "/a/b"; "/a" -> "/").
// View into `p`.
[[nodiscard]] std::string_view parentPath(std::string_view p) noexcept;

[[nodiscard]] bool isAbsolute(std::string_view p) noexcept;

}  // namespace zukiru::filesystem::path
