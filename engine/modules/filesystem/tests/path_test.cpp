#include <zuki/filesystem/path.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zuki::filesystem;

TEST_CASE("normalize collapses . and .. and forces an absolute root", "[filesystem][path]") {
    REQUIRE(path::normalize("assets/textures/hero.png") == "/assets/textures/hero.png");
    REQUIRE(path::normalize("/assets/./textures/../hero.png") == "/assets/hero.png");
    REQUIRE(path::normalize("a//b///c") == "/a/b/c");
    REQUIRE(path::normalize("") == "/");
    REQUIRE(path::normalize("/") == "/");
}

TEST_CASE("normalize clamps .. at the root (no escape)", "[filesystem][path]") {
    REQUIRE(path::normalize("..") == "/");
    REQUIRE(path::normalize("/../../etc/passwd") == "/etc/passwd");
    REQUIRE(path::normalize("assets/../../secret") == "/secret");
}

TEST_CASE("join combines and normalizes", "[filesystem][path]") {
    REQUIRE(path::join("/assets", "textures/hero.png") == "/assets/textures/hero.png");
    REQUIRE(path::join("/assets/models", "../textures/x.png") == "/assets/textures/x.png");
}

TEST_CASE("filename / extension / stem", "[filesystem][path]") {
    REQUIRE(path::filename("/a/b/hero.png") == "hero.png");
    REQUIRE(path::extension("/a/b/hero.png") == ".png");
    REQUIRE(path::stem("/a/b/hero.png") == "hero");

    REQUIRE(path::extension("/a/archive.tar.gz") == ".gz");
    REQUIRE(path::extension("/a/noext") == "");
    REQUIRE(path::extension("/a/.gitignore") == "");  // dotfile, not an extension
    REQUIRE(path::stem("/a/.gitignore") == ".gitignore");
}

TEST_CASE("parentPath", "[filesystem][path]") {
    REQUIRE(path::parentPath("/a/b/c") == "/a/b");
    REQUIRE(path::parentPath("/a") == "/");
    REQUIRE(path::parentPath("/") == "/");
}

TEST_CASE("isAbsolute", "[filesystem][path]") {
    REQUIRE(path::isAbsolute("/assets"));
    REQUIRE_FALSE(path::isAbsolute("assets"));
    REQUIRE_FALSE(path::isAbsolute(""));
}
