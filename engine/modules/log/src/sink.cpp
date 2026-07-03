#include <zukiru/log/sink.hpp>

#include <cstdio>
#include <ctime>
#include <format>

namespace zukiru::log {
namespace {

std::tm localTime(std::time_t t) noexcept {
    std::tm out{};
#if defined(ZUKIRU_OS_WINDOWS)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

}  // namespace

std::string formatRecord(const LogRecord& record, bool withColor) {
    const auto t = std::chrono::system_clock::to_time_t(record.time);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        record.time.time_since_epoch()) %
                    1000;
    const std::tm tm = localTime(t);
    const auto millis = ms.count();

    if (withColor) {
        return std::format("{:02}:{:02}:{:02}.{:03} {}[{}]{} [{}] {} ({}:{})", tm.tm_hour,
                           tm.tm_min, tm.tm_sec, millis, ansiColor(record.level),
                           levelName(record.level), ansiReset(), record.channel, record.message,
                           record.location.file, record.location.line);
    }
    return std::format("{:02}:{:02}:{:02}.{:03} [{}] [{}] {} ({}:{})", tm.tm_hour, tm.tm_min,
                       tm.tm_sec, millis, levelName(record.level), record.channel, record.message,
                       record.location.file, record.location.line);
}

// --- ConsoleSink ----------------------------------------------------------
void ConsoleSink::write(const LogRecord& record) {
    std::string line = formatRecord(record, color_);
    line.push_back('\n');
    std::FILE* out = toStderr_ ? stderr : stdout;
    std::fwrite(line.data(), 1, line.size(), out);
}

void ConsoleSink::flush() {
    std::fflush(toStderr_ ? stderr : stdout);
}

// --- FileSink -------------------------------------------------------------
FileSink::FileSink(const std::string& path, bool append)
    : stream_(path, append ? (std::ios::out | std::ios::app) : std::ios::out) {}

void FileSink::write(const LogRecord& record) {
    if (!stream_.is_open()) return;
    stream_ << formatRecord(record, /*withColor=*/false) << '\n';
}

void FileSink::flush() {
    stream_.flush();
}

}  // namespace zukiru::log
