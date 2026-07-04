#include <zukiru/filesystem/virtual_file_system.hpp>

#include <zukiru/filesystem/path.hpp>
#include <zukiru/platform/file_io.hpp>

#include <format>
#include <utility>

namespace zukiru::filesystem {
namespace stdfs = std::filesystem;

bool FileSystem::mount(std::string_view virtualPrefix, const stdfs::path& realDirectory,
                       bool writable) {
    std::error_code ec;
    if (!stdfs::is_directory(realDirectory, ec)) return false;

    const std::string prefix = path::normalize(virtualPrefix);
    unmount(prefix);  // replace any existing mount at this prefix

    stdfs::path root = stdfs::absolute(realDirectory, ec);
    if (ec) return false;
    mounts_.push_back(Mount{prefix, root.lexically_normal(), writable});
    return true;
}

bool FileSystem::unmount(std::string_view virtualPrefix) {
    const std::string prefix = path::normalize(virtualPrefix);
    for (usize i = 0; i < mounts_.size(); ++i) {
        if (mounts_[i].prefix == prefix) {
            mounts_.erase(mounts_.begin() + static_cast<isize>(i));
            return true;
        }
    }
    return false;
}

bool FileSystem::isMounted(std::string_view virtualPrefix) const {
    const std::string prefix = path::normalize(virtualPrefix);
    for (const Mount& mount : mounts_) {
        if (mount.prefix == prefix) return true;
    }
    return false;
}

const FileSystem::Mount* FileSystem::matchMount(const std::string& normalized,
                                                std::string& relative) const {
    const Mount* best = nullptr;
    usize bestLength = 0;
    for (const Mount& mount : mounts_) {
        const bool matches = mount.prefix == "/" || normalized == mount.prefix ||
                             normalized.starts_with(mount.prefix + "/");
        if (matches && mount.prefix.size() > bestLength) {
            best = &mount;
            bestLength = mount.prefix.size();
        }
    }
    if (best == nullptr) return nullptr;

    relative = normalized.substr(best->prefix.size());
    if (!relative.empty() && relative.front() == '/') relative.erase(0, 1);
    return best;
}

std::optional<stdfs::path> FileSystem::resolve(std::string_view virtualPath) const {
    const std::string normalized = path::normalize(virtualPath);
    std::string relative;
    const Mount* mount = matchMount(normalized, relative);
    if (mount == nullptr) return std::nullopt;

    stdfs::path real = mount->root;
    if (!relative.empty()) real /= relative;
    return real.lexically_normal();
}

bool FileSystem::exists(std::string_view virtualPath) const {
    const auto real = resolve(virtualPath);
    return real && platform::fileExists(*real);
}

std::optional<u64> FileSystem::fileSize(std::string_view virtualPath) const {
    const auto real = resolve(virtualPath);
    if (!real) return std::nullopt;
    return platform::fileSize(*real);
}

Result<std::string> FileSystem::readFile(std::string_view virtualPath) const {
    const auto real = resolve(virtualPath);
    if (!real) {
        return Err(Error{std::format("no mount for virtual path '{}'", virtualPath)});
    }
    return platform::readFile(*real);
}

Result<std::vector<byte>> FileSystem::readFileBinary(std::string_view virtualPath) const {
    const auto real = resolve(virtualPath);
    if (!real) {
        return Err(Error{std::format("no mount for virtual path '{}'", virtualPath)});
    }
    return platform::readFileBinary(*real);
}

Status FileSystem::writeFile(std::string_view virtualPath, std::string_view data, bool append) {
    const std::string normalized = path::normalize(virtualPath);
    std::string relative;
    const Mount* mount = matchMount(normalized, relative);
    if (mount == nullptr) {
        return Err(Error{std::format("no mount for virtual path '{}'", virtualPath)});
    }
    if (!mount->writable) {
        return Err(Error{std::format("mount '{}' is read-only", mount->prefix)});
    }

    stdfs::path real = mount->root;
    if (!relative.empty()) real /= relative;
    real = real.lexically_normal();

    std::error_code ec;
    stdfs::create_directories(real.parent_path(), ec);
    return platform::writeFile(real, data, append);
}

}  // namespace zukiru::filesystem
