#include <zukiru/log/log_level.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zukiru::log;

TEST_CASE("levels are ordered by severity", "[log][level]") {
    STATIC_REQUIRE(LogLevel::Trace < LogLevel::Debug);
    STATIC_REQUIRE(LogLevel::Info < LogLevel::Warn);
    STATIC_REQUIRE(LogLevel::Error < LogLevel::Critical);
    STATIC_REQUIRE(LogLevel::Critical < LogLevel::Off);
}

TEST_CASE("levelName gives aligned labels", "[log][level]") {
    REQUIRE(levelName(LogLevel::Info) == "INFO ");
    REQUIRE(levelName(LogLevel::Error) == "ERROR");
    REQUIRE(levelName(LogLevel::Critical) == "CRIT ");
}

TEST_CASE("parseLevel is case-insensitive and accepts aliases", "[log][level]") {
    REQUIRE(parseLevel("trace") == LogLevel::Trace);
    REQUIRE(parseLevel("INFO") == LogLevel::Info);
    REQUIRE(parseLevel("  Warn ") == LogLevel::Warn);
    REQUIRE(parseLevel("warning") == LogLevel::Warn);
    REQUIRE(parseLevel("critical") == LogLevel::Critical);
    REQUIRE(parseLevel("off") == LogLevel::Off);
    REQUIRE_FALSE(parseLevel("bogus").has_value());
    REQUIRE_FALSE(parseLevel("").has_value());
}
