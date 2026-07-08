#include <zuki/log/sink.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace zuki::log;

namespace {

LogRecord makeRecord(LogLevel level, std::string_view channel, std::string_view message) {
    LogRecord r;
    r.level = level;
    r.channel = channel;
    r.message = message;
    r.location = zuki::SourceLocation{"file.cpp", "fn", 42};
    r.time = std::chrono::system_clock::now();
    return r;
}

}  // namespace

TEST_CASE("formatRecord contains level, channel, message and location", "[log][sink]") {
    const std::string line = formatRecord(makeRecord(LogLevel::Warn, "physics", "collision"),
                                          /*withColor=*/false);
    REQUIRE(line.find("[WARN ]") != std::string::npos);
    REQUIRE(line.find("[physics]") != std::string::npos);
    REQUIRE(line.find("collision") != std::string::npos);
    REQUIRE(line.find("(file.cpp:42)") != std::string::npos);
    // Plain format carries no ANSI escapes.
    REQUIRE(line.find("\x1b[") == std::string::npos);
}

TEST_CASE("formatRecord adds ANSI color when asked", "[log][sink]") {
    const std::string line = formatRecord(makeRecord(LogLevel::Error, "x", "y"), /*withColor=*/true);
    REQUIRE(line.find("\x1b[") != std::string::npos);
}

TEST_CASE("CallbackSink forwards the record", "[log][sink]") {
    int calls = 0;
    LogLevel seen = LogLevel::Off;
    CallbackSink sink([&](const LogRecord& r) {
        ++calls;
        seen = r.level;
    });
    sink.write(makeRecord(LogLevel::Critical, "c", "m"));
    REQUIRE(calls == 1);
    REQUIRE(seen == LogLevel::Critical);
}

TEST_CASE("FileSink writes formatted lines to disk", "[log][sink]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "zuki_log_sink_test.log";
    std::filesystem::remove(path);

    {
        FileSink sink(path.string(), /*append=*/false);
        REQUIRE(sink.isOpen());
        sink.write(makeRecord(LogLevel::Info, "boot", "started"));
        sink.write(makeRecord(LogLevel::Error, "boot", "failed"));
        sink.flush();
    }

    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string contents = buffer.str();
    REQUIRE(contents.find("started") != std::string::npos);
    REQUIRE(contents.find("failed") != std::string::npos);
    // Two records -> two newlines.
    REQUIRE(std::count(contents.begin(), contents.end(), '\n') == 2);

    std::filesystem::remove(path);
}
