#include <zukiru/core/assert.hpp>

#include <cstdio>
#include <cstdlib>

namespace zuki {
namespace {

// Default handler: report to stderr and abort.
void defaultHandler(const SourceLocation& where, std::string_view expr, std::string_view message) {
    std::fputs("\n=== Zukiru assertion failed ===\n", stderr);
    if (!expr.empty()) {
        std::fprintf(stderr, "  condition: %.*s\n", static_cast<int>(expr.size()), expr.data());
    }
    if (!message.empty()) {
        std::fprintf(stderr, "  message:   %.*s\n", static_cast<int>(message.size()),
                     message.data());
    }
    std::fprintf(stderr, "  at:        %s:%d\n  in:        %s\n\n", where.file, where.line,
                 where.function);
    std::fflush(stderr);
}

AssertHandler g_handler = &defaultHandler;

}  // namespace

AssertHandler setAssertHandler(AssertHandler handler) {
    AssertHandler previous = g_handler;
    g_handler = handler != nullptr ? handler : &defaultHandler;
    return previous;
}

namespace detail {

void assertFail(const SourceLocation& where, std::string_view expr, std::string_view message) {
    g_handler(where, expr, message);
    // If the handler returned (i.e. did not throw), terminate — a failed
    // assertion must never let execution continue past this point.
    std::abort();
}

}  // namespace detail
}  // namespace zuki
