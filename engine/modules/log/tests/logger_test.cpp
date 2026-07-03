#include <zukiru/log/logger.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace zukiru::log;

namespace {

struct Captured {
    LogLevel level;
    std::string channel;
    std::string message;
};

// A sink that records structured copies of everything it receives.
std::shared_ptr<CallbackSink> makeCapture(std::vector<Captured>& out) {
    return std::make_shared<CallbackSink>([&out](const LogRecord& r) {
        out.push_back({r.level, std::string{r.channel}, std::string{r.message}});
    });
}

constexpr zukiru::SourceLocation kLoc{"test.cpp", "fn", 1};

}  // namespace

TEST_CASE("isEnabled honors the global threshold", "[log][logger]") {
    Logger logger;
    logger.setLevel(LogLevel::Info);
    REQUIRE_FALSE(logger.isEnabled(LogLevel::Trace, "any"));
    REQUIRE_FALSE(logger.isEnabled(LogLevel::Debug, "any"));
    REQUIRE(logger.isEnabled(LogLevel::Info, "any"));
    REQUIRE(logger.isEnabled(LogLevel::Error, "any"));
}

TEST_CASE("per-channel overrides beat the global threshold", "[log][logger]") {
    Logger logger;
    logger.setLevel(LogLevel::Warn);
    logger.setChannelLevel("verbose", LogLevel::Trace);
    logger.setChannelLevel("quiet", LogLevel::Off);

    REQUIRE(logger.isEnabled(LogLevel::Trace, "verbose"));
    REQUIRE_FALSE(logger.isEnabled(LogLevel::Info, "other"));   // falls back to global Warn
    REQUIRE_FALSE(logger.isEnabled(LogLevel::Error, "quiet"));  // channel forced Off

    logger.clearChannelLevel("verbose");
    REQUIRE_FALSE(logger.isEnabled(LogLevel::Trace, "verbose"));  // back to global Warn
}

TEST_CASE("Off level never emits", "[log][logger]") {
    Logger logger;
    logger.setLevel(LogLevel::Trace);
    REQUIRE_FALSE(logger.isEnabled(LogLevel::Off, "any"));
}

TEST_CASE("log dispatches a structured record to every sink", "[log][logger]") {
    Logger logger;
    std::vector<Captured> a;
    std::vector<Captured> b;
    logger.addSink(makeCapture(a));
    logger.addSink(makeCapture(b));
    REQUIRE(logger.sinkCount() == 2);

    logger.log(LogLevel::Warn, "render", "dropped frame", kLoc);

    REQUIRE(a.size() == 1);
    REQUIRE(b.size() == 1);
    REQUIRE(a[0].level == LogLevel::Warn);
    REQUIRE(a[0].channel == "render");
    REQUIRE(a[0].message == "dropped frame");
}

TEST_CASE("clearSinks removes all outputs", "[log][logger]") {
    Logger logger;
    std::vector<Captured> a;
    logger.addSink(makeCapture(a));
    logger.clearSinks();
    REQUIRE(logger.sinkCount() == 0);
    logger.log(LogLevel::Info, "x", "y", kLoc);
    REQUIRE(a.empty());
}

TEST_CASE("macros format lazily and route through the default logger", "[log][logger][macro]") {
    Logger& logger = defaultLogger();
    logger.clearSinks();
    std::vector<Captured> captured;
    logger.addSink(makeCapture(captured));
    logger.setLevel(LogLevel::Info);

    ZUKIRU_LOG_INFO("test", "value={}", 42);
    ZUKIRU_LOG_WARN("test", "{} items", 3);

    // Raise the "noisy" channel so Info is dropped there but Error still passes.
    logger.setChannelLevel("noisy", LogLevel::Error);
    ZUKIRU_LOG_INFO("noisy", "dropped");
    ZUKIRU_LOG_ERROR("noisy", "kept");

    REQUIRE(captured.size() == 3);
    REQUIRE(captured[0].message == "value=42");
    REQUIRE(captured[0].level == LogLevel::Info);
    REQUIRE(captured[1].message == "3 items");
    REQUIRE(captured[1].level == LogLevel::Warn);
    REQUIRE(captured[2].channel == "noisy");
    REQUIRE(captured[2].message == "kept");
    REQUIRE(captured[2].level == LogLevel::Error);

    // Restore the default console sink for any later use in this process.
    logger.clearSinks();
    logger.clearChannelLevel("noisy");
    logger.addSink(std::make_shared<ConsoleSink>(false));
}
