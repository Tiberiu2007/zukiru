#include <zuki/jobs/job_system.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <future>
#include <vector>

using namespace zuki;
using namespace zuki::jobs;

TEST_CASE("a job system has at least one worker", "[jobs]") {
    JobSystem jobs;
    REQUIRE(jobs.workerCount() >= 1);

    JobSystem four(4);
    REQUIRE(four.workerCount() == 4);
}

TEST_CASE("submit runs every task; waitIdle blocks until done", "[jobs]") {
    JobSystem jobs(4);
    std::atomic<int> counter{0};
    constexpr int kTasks = 1000;
    for (int i = 0; i < kTasks; ++i) {
        jobs.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    jobs.waitIdle();
    REQUIRE(counter.load() == kTasks);
}

TEST_CASE("async returns a future carrying the result", "[jobs]") {
    JobSystem jobs;
    auto future = jobs.async([] { return 6 * 7; });
    REQUIRE(future.get() == 42);
}

TEST_CASE("async propagates arguments captured by the callable", "[jobs]") {
    JobSystem jobs;
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 16; ++i) {
        futures.push_back(jobs.async([i] { return i * i; }));
    }
    for (int i = 0; i < 16; ++i) {
        REQUIRE(futures[static_cast<usize>(i)].get() == i * i);
    }
}

TEST_CASE("parallelFor visits every index exactly once", "[jobs]") {
    JobSystem jobs(4);
    constexpr usize kCount = 10'000;
    std::vector<int> touched(kCount, 0);

    jobs.parallelFor(kCount, 256, [&touched](usize begin, usize end) {
        for (usize i = begin; i < end; ++i) touched[i] += 1;
    });

    REQUIRE(std::all_of(touched.begin(), touched.end(), [](int v) { return v == 1; }));
}

TEST_CASE("parallelFor handles counts not divisible by the chunk size", "[jobs]") {
    JobSystem jobs(3);
    constexpr usize kCount = 1003;  // prime-ish, not a multiple of chunk
    std::atomic<usize> sum{0};

    jobs.parallelFor(kCount, 64, [&sum](usize begin, usize end) {
        usize local = 0;
        for (usize i = begin; i < end; ++i) local += i;
        sum.fetch_add(local, std::memory_order_relaxed);
    });

    // Sum of 0..kCount-1.
    const usize expected = (kCount - 1) * kCount / 2;
    REQUIRE(sum.load() == expected);
}

TEST_CASE("parallelFor with the auto chunk size still covers everything", "[jobs]") {
    JobSystem jobs;
    constexpr usize kCount = 5000;
    std::atomic<usize> count{0};
    jobs.parallelFor(kCount, [&count](usize begin, usize end) {
        count.fetch_add(end - begin, std::memory_order_relaxed);
    });
    REQUIRE(count.load() == kCount);
}

TEST_CASE("parallelFor on an empty range is a no-op", "[jobs]") {
    JobSystem jobs;
    bool called = false;
    jobs.parallelFor(0, 16, [&called](usize, usize) { called = true; });
    REQUIRE_FALSE(called);
}

TEST_CASE("waitIdle with no work returns immediately", "[jobs]") {
    JobSystem jobs;
    jobs.waitIdle();
    SUCCEED();
}
