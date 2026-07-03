#include <zukiru/core/config.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zukiru;

TEST_CASE("set / get round-trips typed values", "[core][config]") {
    Config cfg;
    cfg.setInt("width", 1920);
    cfg.setFloat("scale", 1.5);
    cfg.setBool("vsync", true);
    cfg.setString("title", "Zukiru");

    REQUIRE(cfg.has("width"));
    REQUIRE(cfg.getInt("width") == 1920);
    REQUIRE(cfg.getFloat("scale").value() == 1.5);
    REQUIRE(cfg.getBool("vsync") == true);
    REQUIRE(cfg.getString("title") == "Zukiru");
    REQUIRE(cfg.size() == 4);
}

TEST_CASE("missing keys yield nullopt / fallback", "[core][config]") {
    Config cfg;
    REQUIRE_FALSE(cfg.has("nope"));
    REQUIRE_FALSE(cfg.getInt("nope").has_value());
    REQUIRE(cfg.getIntOr("nope", 7) == 7);
    REQUIRE(cfg.getBoolOr("nope", true) == true);
    REQUIRE(cfg.getStringOr("nope", "def") == "def");
}

TEST_CASE("typed getters reject unparsable values", "[core][config]") {
    Config cfg;
    cfg.setString("x", "not-a-number");
    REQUIRE_FALSE(cfg.getInt("x").has_value());
    REQUIRE_FALSE(cfg.getFloat("x").has_value());
    REQUIRE_FALSE(cfg.getBool("x").has_value());
}

TEST_CASE("bool parsing accepts common spellings", "[core][config]") {
    Config cfg;
    cfg.setString("a", "YES");
    cfg.setString("b", "off");
    cfg.setString("c", "1");
    REQUIRE(cfg.getBool("a") == true);
    REQUIRE(cfg.getBool("b") == false);
    REQUIRE(cfg.getBool("c") == true);
}

TEST_CASE("remove deletes a key", "[core][config]") {
    Config cfg;
    cfg.setInt("k", 1);
    REQUIRE(cfg.remove("k"));
    REQUIRE_FALSE(cfg.has("k"));
    REQUIRE_FALSE(cfg.remove("k"));
}

TEST_CASE("loadFromString parses key=value with comments", "[core][config]") {
    Config cfg;
    const auto status = cfg.loadFromString(
        "# a comment\n"
        "; another comment\n"
        "\n"
        "  width = 800 \n"
        "title=Hello World\n");
    REQUIRE(status.isOk());
    REQUIRE(cfg.getInt("width") == 800);
    REQUIRE(cfg.getString("title") == "Hello World");
    REQUIRE(cfg.size() == 2);
}

TEST_CASE("loadFromString reports malformed lines", "[core][config]") {
    Config cfg;
    const auto status = cfg.loadFromString("valid = 1\ngarbage-without-equals\n");
    REQUIRE(status.isErr());
    REQUIRE(status.error().message.find("line 2") != std::string::npos);
}

TEST_CASE("toString is sorted and reloadable", "[core][config]") {
    Config cfg;
    cfg.setInt("bbb", 2);
    cfg.setInt("aaa", 1);
    const std::string text = cfg.toString();
    REQUIRE(text == "aaa = 1\nbbb = 2\n");

    Config reloaded;
    REQUIRE(reloaded.loadFromString(text).isOk());
    REQUIRE(reloaded.getInt("aaa") == 1);
    REQUIRE(reloaded.getInt("bbb") == 2);
}
