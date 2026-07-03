#include <zukiru/platform/thread.hpp>

#include <catch2/catch_test_macros.hpp>

#include <thread>

using namespace zukiru;
using namespace zukiru::platform;

TEST_CASE("hardwareConcurrency is at least one", "[platform][thread]") {
    REQUIRE(hardwareConcurrency() >= 1);
}

TEST_CASE("currentThreadId is stable within a thread and differs across threads",
          "[platform][thread]") {
    const u64 here = currentThreadId();
    REQUIRE(here == currentThreadId());

    u64 other = here;
    std::thread t([&other] { other = currentThreadId(); });
    t.join();
    REQUIRE(other != here);
}

TEST_CASE("yieldThread does not crash", "[platform][thread]") {
    yieldThread();
    SUCCEED();
}

#if defined(ZUKIRU_OS_LINUX) || defined(ZUKIRU_OS_MACOS)
TEST_CASE("thread name round-trips on POSIX", "[platform][thread]") {
    REQUIRE(setThreadName("zk-worker"));
    REQUIRE(threadName() == "zk-worker");
}

TEST_CASE("thread name is truncated to the platform limit", "[platform][thread]") {
    // Longer than Linux's 15-char limit; must not fail, just truncate.
    REQUIRE(setThreadName("a-very-long-thread-name"));
    REQUIRE(threadName().size() <= 15);
}
#endif
