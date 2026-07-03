// RingBuffer<T> — a fixed-capacity circular FIFO queue.
//
// push() fails when full; pushOverwrite() drops the oldest element to make room
// (handy for bounded history / logs / lock-free-ish producer-consumer patterns).
// pop() removes from the front. All operations are O(1).
#pragma once

#include <zukiru/core/assert.hpp>
#include <zukiru/core/types.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace zukiru::containers {

template <class T>
class RingBuffer {
public:
    explicit RingBuffer(usize capacity) : slots_(capacity == 0 ? 1 : capacity) {
        ZUKIRU_ENSURE_MSG(capacity > 0, "RingBuffer capacity must be > 0");
    }

    [[nodiscard]] usize capacity() const noexcept { return slots_.size(); }
    [[nodiscard]] usize size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0; }
    [[nodiscard]] bool full() const noexcept { return count_ == slots_.size(); }

    // Enqueue at the back. Returns false (without inserting) if full.
    bool push(T value) {
        if (full()) return false;
        const usize tail = (head_ + count_) % slots_.size();
        slots_[tail] = std::move(value);
        ++count_;
        return true;
    }

    // Enqueue at the back, dropping the oldest element if already full.
    void pushOverwrite(T value) {
        if (full()) {
            slots_[head_] = std::move(value);          // overwrite oldest...
            head_ = (head_ + 1) % slots_.size();       // ...which becomes newest
        } else {
            const usize tail = (head_ + count_) % slots_.size();
            slots_[tail] = std::move(value);
            ++count_;
        }
    }

    // Dequeue from the front. Returns std::nullopt if empty.
    [[nodiscard]] std::optional<T> pop() {
        if (empty()) return std::nullopt;
        std::optional<T> out = std::move(slots_[head_]);
        slots_[head_].reset();
        head_ = (head_ + 1) % slots_.size();
        --count_;
        return out;
    }

    // Peek without removing (nullptr if empty).
    [[nodiscard]] T* front() noexcept { return empty() ? nullptr : &*slots_[head_]; }
    [[nodiscard]] T* back() noexcept {
        if (empty()) return nullptr;
        const usize tail = (head_ + count_ - 1) % slots_.size();
        return &*slots_[tail];
    }

    void clear() noexcept {
        for (auto& slot : slots_) slot.reset();
        head_ = 0;
        count_ = 0;
    }

private:
    std::vector<std::optional<T>> slots_;
    usize head_ = 0;
    usize count_ = 0;
};

}  // namespace zukiru::containers
