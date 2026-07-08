#include <zuki/platform/clock.hpp>

#include <chrono>
#include <thread>

namespace zuki::platform {

void sleepFor(Duration duration) {
    if (duration.nanos() <= 0) return;
    std::this_thread::sleep_for(std::chrono::nanoseconds(duration.nanos()));
}

void sleepMilliseconds(u32 milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

u64 performanceCounter() noexcept {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    return static_cast<u64>(ns);
}

u64 performanceFrequency() noexcept {
    return 1'000'000'000ull;  // performanceCounter() is denominated in nanoseconds
}

u64 unixTimeSeconds() noexcept {
    return static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count());
}

u64 unixTimeMilliseconds() noexcept {
    return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count());
}

}  // namespace zuki::platform
