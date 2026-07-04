// JobSystem — a work-stealing-free thread pool with fire-and-forget tasks,
// futures, and a blocking parallel-for.
//
//   JobSystem jobs;                       // one worker per hardware thread
//   jobs.submit([]{ doWork(); });         // fire-and-forget
//   auto f = jobs.async([]{ return 42; }); // future result
//   jobs.parallelFor(n, [](usize i0, usize i1){ for (usize i=i0;i<i1;++i) ...; });
//   jobs.waitIdle();                      // block until all queued work is done
//
// A thread that calls parallelFor/waitIdle helps run pending tasks instead of
// merely blocking, so the pool can't deadlock on nested dispatch and no core
// sits idle while there is work to do.
#pragma once

#include <zukiru/core/types.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace zukiru::jobs {

class JobSystem {
public:
    // workerCount == 0 selects one worker per hardware thread.
    explicit JobSystem(u32 workerCount = 0);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    [[nodiscard]] u32 workerCount() const noexcept { return static_cast<u32>(workers_.size()); }

    // Enqueue a fire-and-forget task.
    void submit(std::function<void()> task);

    // Enqueue a task and get a future for its result.
    template <class F>
    [[nodiscard]] auto async(F&& fn) -> std::future<std::invoke_result_t<F>> {
        using R = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(fn));
        std::future<R> future = task->get_future();
        submit([task] { (*task)(); });
        return future;
    }

    // Split [0, count) into chunks of ~chunkSize and run `body(begin, end)` for
    // each across the pool. Blocks (helping) until every chunk completes.
    void parallelFor(usize count, usize chunkSize,
                     const std::function<void(usize begin, usize end)>& body);

    // parallelFor with an automatically chosen chunk size.
    void parallelFor(usize count, const std::function<void(usize begin, usize end)>& body);

    // Block until the queue is empty and no task is executing (helping meanwhile).
    void waitIdle();

private:
    std::function<void()> nextTask();      // pop one task, or empty if none
    bool tryRunOne();                      // run one pending task; false if none
    void workerLoop(u32 index);

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex mutex_;
    std::condition_variable workAvailable_;
    std::condition_variable idle_;
    usize activeTasks_ = 0;
    bool stop_ = false;
};

}  // namespace zukiru::jobs
