#include <zuki/log/logger.hpp>

#include <thread>
#include <utility>

namespace zuki::log {

void Logger::addSink(std::shared_ptr<Sink> sink) {
    if (!sink) return;
    const std::lock_guard lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clearSinks() {
    const std::lock_guard lock(mutex_);
    sinks_.clear();
}

usize Logger::sinkCount() const {
    const std::lock_guard lock(mutex_);
    return sinks_.size();
}

void Logger::setLevel(LogLevel level) {
    const std::lock_guard lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const {
    const std::lock_guard lock(mutex_);
    return level_;
}

void Logger::setChannelLevel(std::string_view channel, LogLevel level) {
    const std::lock_guard lock(mutex_);
    channelLevels_[std::string{channel}] = level;
}

void Logger::clearChannelLevel(std::string_view channel) {
    const std::lock_guard lock(mutex_);
    const auto it = channelLevels_.find(channel);
    if (it != channelLevels_.end()) channelLevels_.erase(it);
}

bool Logger::isEnabled(LogLevel level, std::string_view channel) const {
    if (level == LogLevel::Off) return false;
    const std::lock_guard lock(mutex_);
    LogLevel threshold = level_;
    const auto it = channelLevels_.find(channel);
    if (it != channelLevels_.end()) threshold = it->second;
    return level >= threshold;
}

void Logger::log(LogLevel level, std::string_view channel, std::string message,
                 SourceLocation location) {
    LogRecord record;
    record.level = level;
    record.channel = channel;
    record.message = message;  // view into `message`, valid for this dispatch
    record.location = location;
    record.time = std::chrono::system_clock::now();
    record.threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());

    const std::lock_guard lock(mutex_);
    for (const auto& sink : sinks_) sink->write(record);
}

void Logger::flush() {
    const std::lock_guard lock(mutex_);
    for (const auto& sink : sinks_) sink->flush();
}

Logger& defaultLogger() {
    // Leaked on purpose: a never-destroyed singleton avoids static-destruction
    // order hazards with code that logs during shutdown.
    static Logger* const instance = [] {
        auto* logger = new Logger();
        logger->addSink(std::make_shared<ConsoleSink>(/*color=*/false));
        return logger;
    }();
    return *instance;
}

}  // namespace zuki::log
