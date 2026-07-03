#include <zukiru/platform/file_io.hpp>

#include <format>
#include <fstream>

namespace zukiru::platform {

Result<std::string> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return Err(Error{std::format("cannot open '{}' for reading", path.string())});
    }
    const std::streamoff size = stream.tellg();
    std::string contents;
    if (size > 0) {
        contents.resize(static_cast<usize>(size));
        stream.seekg(0);
        stream.read(contents.data(), size);
    }
    return Ok(std::move(contents));
}

Result<std::vector<byte>> readFileBinary(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return Err(Error{std::format("cannot open '{}' for reading", path.string())});
    }
    const std::streamoff size = stream.tellg();
    std::vector<byte> buffer;
    if (size > 0) {
        buffer.resize(static_cast<usize>(size));
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(buffer.data()), size);
    }
    return Ok(std::move(buffer));
}

Status writeFile(const std::filesystem::path& path, std::string_view data, bool append) {
    const std::ios::openmode mode =
        std::ios::binary | (append ? std::ios::app : std::ios::trunc);
    std::ofstream stream(path, mode);
    if (!stream) {
        return Err(Error{std::format("cannot open '{}' for writing", path.string())});
    }
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!stream) {
        return Err(Error{std::format("write to '{}' failed", path.string())});
    }
    return Ok();
}

bool fileExists(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

std::optional<u64> fileSize(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return std::nullopt;
    return static_cast<u64>(size);
}

bool removeFile(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

}  // namespace zukiru::platform
