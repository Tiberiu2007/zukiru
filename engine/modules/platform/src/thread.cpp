#include <zukiru/platform/thread.hpp>

#include <functional>
#include <thread>

#if defined(ZUKIRU_OS_LINUX) || defined(ZUKIRU_OS_MACOS)
#include <pthread.h>
#endif

namespace zukiru::platform {

u32 hardwareConcurrency() noexcept {
    const u32 count = std::thread::hardware_concurrency();
    return count == 0 ? 1u : count;
}

u64 currentThreadId() noexcept {
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

void yieldThread() noexcept {
    std::this_thread::yield();
}

bool setThreadName([[maybe_unused]] std::string_view name) {
#if defined(ZUKIRU_OS_LINUX)
    // Linux caps thread names at 16 bytes including the null terminator.
    char buffer[16];
    const usize count = name.size() < 15 ? name.size() : 15;
    name.copy(buffer, count);
    buffer[count] = '\0';
    return pthread_setname_np(pthread_self(), buffer) == 0;
#elif defined(ZUKIRU_OS_MACOS)
    const std::string owned{name};
    return pthread_setname_np(owned.c_str()) == 0;
#else
    return false;  // unsupported on this platform
#endif
}

std::string threadName() {
#if defined(ZUKIRU_OS_LINUX) || defined(ZUKIRU_OS_MACOS)
    char buffer[64] = {};
    if (pthread_getname_np(pthread_self(), buffer, sizeof(buffer)) == 0) {
        return std::string{buffer};
    }
#endif
    return {};
}

}  // namespace zukiru::platform
