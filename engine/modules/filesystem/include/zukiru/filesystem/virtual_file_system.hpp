// FileSystem — a virtual file system. Real host directories are mounted under
// virtual prefixes; virtual paths (always '/'-separated, rooted at the VFS root)
// are resolved to real paths by longest-matching mount. This decouples game code
// from where assets actually live and makes '..' traversal outside a mount
// impossible (paths are normalized/clamped before resolution).
//
//   FileSystem vfs;
//   vfs.mount("/assets", "/opt/game/assets");
//   vfs.mount("/user",   userDir, /*writable=*/true);
//   auto text = vfs.readFile("/assets/config.ini");
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zukiru::filesystem {

class FileSystem {
public:
    // Mount a real directory under `virtualPrefix` (e.g. "/assets"). Writes are
    // only permitted through mounts created with `writable = true`. Returns false
    // if `realDirectory` is not an existing directory. Re-mounting a prefix
    // replaces the previous mount.
    bool mount(std::string_view virtualPrefix, const std::filesystem::path& realDirectory,
               bool writable = false);
    bool unmount(std::string_view virtualPrefix);
    [[nodiscard]] bool isMounted(std::string_view virtualPrefix) const;
    [[nodiscard]] usize mountCount() const noexcept { return mounts_.size(); }

    // Map a virtual path to a real host path (nullopt if no mount matches).
    [[nodiscard]] std::optional<std::filesystem::path> resolve(std::string_view virtualPath) const;

    [[nodiscard]] bool exists(std::string_view virtualPath) const;
    [[nodiscard]] std::optional<u64> fileSize(std::string_view virtualPath) const;

    [[nodiscard]] Result<std::string> readFile(std::string_view virtualPath) const;
    [[nodiscard]] Result<std::vector<byte>> readFileBinary(std::string_view virtualPath) const;

    // Write through a writable mount, creating parent directories as needed.
    [[nodiscard]] Status writeFile(std::string_view virtualPath, std::string_view data,
                                   bool append = false);

private:
    struct Mount {
        std::string prefix;             // normalized, e.g. "/assets"
        std::filesystem::path root;     // absolute host directory
        bool writable;
    };

    // Longest-prefix match. On success returns the mount and sets `relative` to
    // the path within it (no leading '/').
    const Mount* matchMount(const std::string& normalized, std::string& relative) const;

    std::vector<Mount> mounts_;
};

}  // namespace zukiru::filesystem
