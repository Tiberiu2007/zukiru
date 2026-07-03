// MemoryTracker — lightweight, thread-safe allocation accounting.
//
// Feed it recordAllocation()/recordDeallocation() calls (from an allocator, a
// custom operator new, or manually) and read back live/peak/total figures. All
// counters are lock-free atomics.
#pragma once

#include <zukiru/core/types.hpp>

#include <atomic>

namespace zukiru::memory {

class MemoryTracker {
public:
    MemoryTracker() = default;
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;

    void recordAllocation(usize bytes) noexcept {
        bytesInUse_.fetch_add(bytes, std::memory_order_relaxed);
        liveAllocations_.fetch_add(1, std::memory_order_relaxed);
        totalAllocations_.fetch_add(1, std::memory_order_relaxed);
        totalBytes_.fetch_add(bytes, std::memory_order_relaxed);

        // Bump the peak if we exceeded it (lock-free CAS loop).
        const usize current = bytesInUse_.load(std::memory_order_relaxed);
        usize peak = peakBytes_.load(std::memory_order_relaxed);
        while (current > peak &&
               !peakBytes_.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
        }
    }

    void recordDeallocation(usize bytes) noexcept {
        bytesInUse_.fetch_sub(bytes, std::memory_order_relaxed);
        liveAllocations_.fetch_sub(1, std::memory_order_relaxed);
    }

    [[nodiscard]] usize bytesInUse() const noexcept {
        return bytesInUse_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] usize peakBytes() const noexcept {
        return peakBytes_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] u64 liveAllocations() const noexcept {
        return liveAllocations_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] u64 totalAllocations() const noexcept {
        return totalAllocations_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] u64 totalBytesAllocated() const noexcept {
        return totalBytes_.load(std::memory_order_relaxed);
    }

    void reset() noexcept {
        bytesInUse_.store(0, std::memory_order_relaxed);
        peakBytes_.store(0, std::memory_order_relaxed);
        liveAllocations_.store(0, std::memory_order_relaxed);
        totalAllocations_.store(0, std::memory_order_relaxed);
        totalBytes_.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<usize> bytesInUse_{0};
    std::atomic<usize> peakBytes_{0};
    std::atomic<u64> liveAllocations_{0};
    std::atomic<u64> totalAllocations_{0};
    std::atomic<u64> totalBytes_{0};
};

}  // namespace zukiru::memory
