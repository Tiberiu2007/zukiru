#include <zukiru/filesystem/virtual_file_system.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

using namespace zukiru;
using namespace zukiru::filesystem;
namespace stdfs = std::filesystem;

namespace {

// A unique temporary directory, cleaned up on destruction.
struct TempDir {
    stdfs::path path;
    TempDir() {
        static std::atomic<u64> counter{0};
        std::random_device rd;
        path = stdfs::temp_directory_path() /
               ("zukiru_vfs_" + std::to_string(rd()) + "_" + std::to_string(counter.fetch_add(1)));
        stdfs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        stdfs::remove_all(path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

void writeReal(const stdfs::path& file, std::string_view contents) {
    stdfs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << contents;
}

}  // namespace

TEST_CASE("mount, resolve and read a real file", "[filesystem][vfs]") {
    TempDir tmp;
    writeReal(tmp.path / "data" / "hello.txt", "hi vfs");

    FileSystem vfs;
    REQUIRE(vfs.mount("/assets", tmp.path));
    REQUIRE(vfs.mountCount() == 1);
    REQUIRE(vfs.isMounted("/assets"));

    REQUIRE(vfs.exists("/assets/data/hello.txt"));
    REQUIRE(vfs.fileSize("/assets/data/hello.txt") == 6);

    const auto content = vfs.readFile("/assets/data/hello.txt");
    REQUIRE(content.isOk());
    REQUIRE(content.value() == "hi vfs");
}

TEST_CASE("mounting a nonexistent directory fails", "[filesystem][vfs]") {
    TempDir tmp;
    FileSystem vfs;
    REQUIRE_FALSE(vfs.mount("/x", tmp.path / "does_not_exist"));
    REQUIRE(vfs.mountCount() == 0);
}

TEST_CASE("unresolved virtual paths error cleanly", "[filesystem][vfs]") {
    FileSystem vfs;
    REQUIRE_FALSE(vfs.resolve("/nope/file.txt").has_value());
    REQUIRE(vfs.readFile("/nope/file.txt").isErr());
    REQUIRE_FALSE(vfs.exists("/nope/file.txt"));
    REQUIRE_FALSE(vfs.fileSize("/nope/file.txt").has_value());
}

TEST_CASE("writes go through writable mounts only", "[filesystem][vfs]") {
    TempDir tmp;
    FileSystem vfs;
    vfs.mount("/ro", tmp.path);                 // read-only
    vfs.mount("/rw", tmp.path, /*writable=*/true);

    // Writable mount: creates nested dirs and round-trips.
    REQUIRE(vfs.writeFile("/rw/out/data.txt", "written").isOk());
    REQUIRE(vfs.readFile("/rw/out/data.txt").value() == "written");

    // Append.
    REQUIRE(vfs.writeFile("/rw/out/data.txt", "!", /*append=*/true).isOk());
    REQUIRE(vfs.readFile("/rw/out/data.txt").value() == "written!");

    // Read-only mount rejects writes.
    REQUIRE(vfs.writeFile("/ro/blocked.txt", "no").isErr());
}

TEST_CASE("longest matching mount wins", "[filesystem][vfs]") {
    TempDir base;
    TempDir overlay;
    writeReal(base.path / "textures" / "hero.png", "BASE");
    writeReal(overlay.path / "hero.png", "OVERLAY");

    FileSystem vfs;
    vfs.mount("/assets", base.path);
    vfs.mount("/assets/textures", overlay.path);  // longer prefix

    // Should resolve through the longer "/assets/textures" mount, not "/assets".
    REQUIRE(vfs.readFile("/assets/textures/hero.png").value() == "OVERLAY");
}

TEST_CASE("'..' cannot escape a mount", "[filesystem][vfs]") {
    TempDir tmp;
    writeReal(tmp.path / "secret.txt", "SECRET");
    writeReal(tmp.path / "pub" / "ok.txt", "OK");

    FileSystem vfs;
    vfs.mount("/pub", tmp.path / "pub");

    REQUIRE(vfs.readFile("/pub/ok.txt").value() == "OK");
    // Normalizes to "/secret.txt", which is outside any mount -> unreachable.
    REQUIRE(vfs.readFile("/pub/../secret.txt").isErr());
}

TEST_CASE("unmount and re-mount replacement", "[filesystem][vfs]") {
    TempDir a;
    TempDir b;
    writeReal(a.path / "who.txt", "A");
    writeReal(b.path / "who.txt", "B");

    FileSystem vfs;
    vfs.mount("/m", a.path);
    REQUIRE(vfs.readFile("/m/who.txt").value() == "A");

    vfs.mount("/m", b.path);  // re-mount replaces
    REQUIRE(vfs.mountCount() == 1);
    REQUIRE(vfs.readFile("/m/who.txt").value() == "B");

    REQUIRE(vfs.unmount("/m"));
    REQUIRE_FALSE(vfs.isMounted("/m"));
    REQUIRE_FALSE(vfs.unmount("/m"));
}
