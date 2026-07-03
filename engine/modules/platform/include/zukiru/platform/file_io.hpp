// Blocking file I/O primitives. Small, synchronous helpers over std::filesystem;
// the async, virtual-filesystem layer is a Layer-1 concern (`filesystem` module).
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zukiru::platform {

// Read an entire file into a string (binary-safe). Error if it can't be opened.
[[nodiscard]] Result<std::string> readFile(const std::filesystem::path& path);

// Read an entire file into a byte buffer.
[[nodiscard]] Result<std::vector<byte>> readFileBinary(const std::filesystem::path& path);

// Write `data` to a file, truncating (default) or appending. Creates the file if
// needed. Error if it can't be opened for writing.
[[nodiscard]] Status writeFile(const std::filesystem::path& path, std::string_view data,
                               bool append = false);

// True if the path exists and is a regular file.
[[nodiscard]] bool fileExists(const std::filesystem::path& path) noexcept;

// Size in bytes, or nullopt if the file doesn't exist / can't be queried.
[[nodiscard]] std::optional<u64> fileSize(const std::filesystem::path& path) noexcept;

// Delete a file. Returns true if a file was removed.
bool removeFile(const std::filesystem::path& path) noexcept;

}  // namespace zukiru::platform
