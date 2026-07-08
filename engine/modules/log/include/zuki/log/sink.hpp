// Log sinks — output destinations for LogRecords.
//
// Sinks are always invoked while the owning Logger holds its mutex, so a sink's
// write()/flush() are already serialized and need no internal locking.
#pragma once

#include <zuki/log/log_record.hpp>

#include <functional>
#include <fstream>
#include <string>

namespace zuki::log {

// Render a record to a single line (no trailing newline):
//   HH:MM:SS.mmm [INFO ] [channel] message (file:line)
// When `withColor` is set, the level label is wrapped in ANSI color codes.
[[nodiscard]] std::string formatRecord(const LogRecord& record, bool withColor);

// Abstract output destination.
class Sink {
public:
    Sink() = default;
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    virtual ~Sink() = default;

    virtual void write(const LogRecord& record) = 0;
    virtual void flush() {}
};

// Writes formatted lines to stderr (default) or stdout, optionally colorized.
class ConsoleSink final : public Sink {
public:
    explicit ConsoleSink(bool color = false, bool toStderr = true) noexcept
        : color_(color), toStderr_(toStderr) {}

    void write(const LogRecord& record) override;
    void flush() override;

private:
    bool color_;
    bool toStderr_;
};

// Appends formatted lines to a file.
class FileSink final : public Sink {
public:
    explicit FileSink(const std::string& path, bool append = true);

    [[nodiscard]] bool isOpen() const { return stream_.is_open(); }

    void write(const LogRecord& record) override;
    void flush() override;

private:
    std::ofstream stream_;
};

// Forwards each record to a user callback. Useful for tests and custom routing.
class CallbackSink final : public Sink {
public:
    using Callback = std::function<void(const LogRecord&)>;
    explicit CallbackSink(Callback callback) : callback_(std::move(callback)) {}

    void write(const LogRecord& record) override {
        if (callback_) callback_(record);
    }

private:
    Callback callback_;
};

}  // namespace zuki::log
