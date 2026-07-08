#include <zuki/platform/file_io.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace zuki;
using namespace zuki::platform;

namespace {

std::filesystem::path tempPath(std::string_view name) {
    return std::filesystem::temp_directory_path() / name;
}

}  // namespace

TEST_CASE("write then read round-trips content", "[platform][file_io]") {
    const auto path = tempPath("zuki_fileio_rw.txt");
    removeFile(path);

    REQUIRE(writeFile(path, "hello world").isOk());
    REQUIRE(fileExists(path));
    REQUIRE(fileSize(path) == 11);

    const auto read = readFile(path);
    REQUIRE(read.isOk());
    REQUIRE(read.value() == "hello world");

    removeFile(path);
}

TEST_CASE("append adds to existing content", "[platform][file_io]") {
    const auto path = tempPath("zuki_fileio_append.txt");
    removeFile(path);
    REQUIRE(writeFile(path, "abc").isOk());
    REQUIRE(writeFile(path, "def", /*append=*/true).isOk());
    REQUIRE(readFile(path).value() == "abcdef");
    removeFile(path);
}

TEST_CASE("reading a missing file returns an error", "[platform][file_io]") {
    const auto result = readFile(tempPath("zuki_definitely_missing_xyz.txt"));
    REQUIRE(result.isErr());
    REQUIRE_FALSE(result.error().message.empty());
}

TEST_CASE("queries on a missing file", "[platform][file_io]") {
    const auto path = tempPath("zuki_missing_query.txt");
    removeFile(path);
    REQUIRE_FALSE(fileExists(path));
    REQUIRE_FALSE(fileSize(path).has_value());
    REQUIRE_FALSE(removeFile(path));  // nothing to remove
}

TEST_CASE("binary read yields the raw bytes", "[platform][file_io]") {
    const auto path = tempPath("zuki_fileio_bin.dat");
    removeFile(path);
    REQUIRE(writeFile(path, std::string_view{"\x01\x02\x03\x04", 4}).isOk());

    const auto bytes = readFileBinary(path);
    REQUIRE(bytes.isOk());
    REQUIRE(bytes.value().size() == 4);
    REQUIRE(bytes.value()[0] == byte{0x01});
    REQUIRE(bytes.value()[3] == byte{0x04});

    removeFile(path);
}

TEST_CASE("writing an empty file yields size zero", "[platform][file_io]") {
    const auto path = tempPath("zuki_fileio_empty.txt");
    removeFile(path);
    REQUIRE(writeFile(path, "").isOk());
    REQUIRE(fileExists(path));
    REQUIRE(fileSize(path) == 0);
    REQUIRE(readFile(path).value().empty());
    removeFile(path);
}
