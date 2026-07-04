#include <zukiru/jobs/job_system.hpp>

#include <zukiru/platform/thread.hpp>

#include <format>

namespace zukiru::jobs {

JobSystem::JobSystem(u32 workerCount) {
    const u32 count = workerCount == 0 ? platform::hardwareConcurrency() : workerCount;
    workers_.reserve(count);
    for (u32 i = 0; i < count; ++i) {
        workers_.emplace_back([this, i] { workerLoop(i); });
    }
}

JobSystem::~JobSystem() {
    {
        const std::lock_guard lock(mutex_);
        stop_ = true;
    }
    workAvailable_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void JobSystem::submit(std::function<void()> task) {
    {
        const std::lock_guard lock(mutex_);
        tasks_.push(std::move(task));
    }
    workAvailable_.notify_one();
}

std::function<void()> JobSystem::nextTask() {
    std::function<void()> task;
    if (!tasks_.empty()) {
        task = std::move(tasks_.front());
        tasks_.pop();
    }
    return task;
}

bool JobSystem::tryRunOne() {
    std::function<void()> task;
    {
        std::unique_lock lock(mutex_);
        if (tasks_.empty()) return false;
        task = std::move(tasks_.front());
        tasks_.pop();
        ++activeTasks_;
    }
    task();
    {
        const std::lock_guard lock(mutex_);
        --activeTasks_;
        if (tasks_.empty() && activeTasks_ == 0) idle_.notify_all();
    }
    return true;
}

void JobSystem::workerLoop(u32 index) {
    platform::setThreadName(std::format("zk-worker-{}", index));
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            workAvailable_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
            ++activeTasks_;
        }
        task();
        {
            const std::lock_guard lock(mutex_);
            --activeTasks_;
            if (tasks_.empty() && activeTasks_ == 0) idle_.notify_all();
        }
    }
}

void JobSystem::waitIdle() {
    // Help drain the queue, then wait for any still-executing tasks to finish.
    while (tryRunOne()) {
    }
    std::unique_lock lock(mutex_);
    idle_.wait(lock, [this] { return tasks_.empty() && activeTasks_ == 0; });
}

void JobSystem::parallelFor(usize count, usize chunkSize,
                            const std::function<void(usize, usize)>& body) {
    if (count == 0) return;
    if (chunkSize == 0) chunkSize = 1;

    const usize chunks = (count + chunkSize - 1) / chunkSize;
    std::atomic<usize> remaining{chunks};

    for (usize c = 0; c < chunks; ++c) {
        const usize begin = c * chunkSize;
        const usize end = begin + chunkSize < count ? begin + chunkSize : count;
        submit([&body, &remaining, begin, end] {
            body(begin, end);
            remaining.fetch_sub(1, std::memory_order_release);
        });
    }

    // Help run tasks until this batch is done (safe even if called from a worker).
    while (remaining.load(std::memory_order_acquire) != 0) {
        if (!tryRunOne()) std::this_thread::yield();
    }
}

void JobSystem::parallelFor(usize count, const std::function<void(usize, usize)>& body) {
    // Aim for a few chunks per worker to balance load without excessive overhead.
    const usize workers = workers_.empty() ? 1 : workers_.size();
    const usize target = workers * 4;
    const usize chunkSize = count < target ? 1 : (count + target - 1) / target;
    parallelFor(count, chunkSize, body);
}

}  // namespace zukiru::jobs
