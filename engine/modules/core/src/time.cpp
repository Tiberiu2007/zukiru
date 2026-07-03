#include <zukiru/core/time.hpp>

namespace zuki {

Instant Clock::now() {
    return Instant{std::chrono::steady_clock::now()};
}

Duration Instant::elapsed() const {
    return Clock::now() - *this;
}

}  // namespace zuki
