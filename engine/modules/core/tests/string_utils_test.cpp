#include <zuki/core/string_utils.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zuki;
namespace s = zuki::strings;

TEST_CASE("trim removes surrounding whitespace", "[core][strings]") {
    REQUIRE(s::trim("  hi \t\n") == "hi");
    REQUIRE(s::trimLeft("  hi") == "hi");
    REQUIRE(s::trimRight("hi  ") == "hi");
    REQUIRE(s::trim("   ").empty());
    REQUIRE(s::trim("nochange") == "nochange");
}

TEST_CASE("case conversion is ASCII", "[core][strings]") {
    REQUIRE(s::toLower("HeLLo123") == "hello123");
    REQUIRE(s::toUpper("HeLLo123") == "HELLO123");
}

TEST_CASE("predicates", "[core][strings]") {
    REQUIRE(s::contains("abcdef", "cde"));
    REQUIRE_FALSE(s::contains("abcdef", "xyz"));
    REQUIRE(s::equalsIgnoreCase("Engine", "ENGINE"));
    REQUIRE_FALSE(s::equalsIgnoreCase("Engine", "engin"));
}

TEST_CASE("split on a char", "[core][strings]") {
    const auto parts = s::split("a,b,c", ',');
    REQUIRE(parts.size() == 3);
    REQUIRE(parts[0] == "a");
    REQUIRE(parts[2] == "c");
}

TEST_CASE("split preserves or skips empty fields", "[core][strings]") {
    REQUIRE(s::split("a,,c", ',').size() == 3);
    REQUIRE(s::split("a,,c", ',', /*skipEmpty=*/true).size() == 2);
    REQUIRE(s::split("", ',').size() == 1);
    REQUIRE(s::split("", ',', true).empty());
}

TEST_CASE("split on a multi-char delimiter", "[core][strings]") {
    const auto parts = s::split("a::b::c", "::");
    REQUIRE(parts.size() == 3);
    REQUIRE(parts[1] == "b");
}

TEST_CASE("join is the inverse of split", "[core][strings]") {
    const auto parts = s::split("x-y-z", '-');
    REQUIRE(s::join(parts, "-") == "x-y-z");
    REQUIRE(s::join(std::vector<std::string>{}, ",").empty());
}

TEST_CASE("replaceAll replaces every occurrence", "[core][strings]") {
    REQUIRE(s::replaceAll("aXbXc", "X", "--") == "a--b--c");
    REQUIRE(s::replaceAll("hello", "l", "L") == "heLLo");
    REQUIRE(s::replaceAll("hello", "z", "?") == "hello");
    REQUIRE(s::replaceAll("hello", "", "?") == "hello");
}
