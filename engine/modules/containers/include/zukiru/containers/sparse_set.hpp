// SparseSet<T> — a sparse-to-dense map from small u32 keys to values.
//
// Keys index a sparse array that points into a packed (dense) array of values,
// so: O(1) insert/remove/lookup, values stored contiguously for fast iteration,
// and removal is a cheap swap-with-last (order is not preserved). This is the
// classic building block behind ECS component storage.
//
// Trade-off: the sparse array grows to `maxKey + 1`, so keys should be dense-ish
// small integers, not arbitrary hashes.
#pragma once

#include <zukiru/core/types.hpp>

#include <utility>
#include <vector>

namespace zukiru::containers {

template <class T>
class SparseSet {
public:
    using Key = u32;
    static constexpr Key kInvalid = 0xFFFF'FFFFu;

    [[nodiscard]] bool contains(Key key) const noexcept {
        return key < sparse_.size() && sparse_[key] < dense_.size() && dense_[sparse_[key]] == key;
    }

    [[nodiscard]] usize size() const noexcept { return dense_.size(); }
    [[nodiscard]] bool empty() const noexcept { return dense_.empty(); }

    void reserve(usize count) {
        dense_.reserve(count);
        values_.reserve(count);
    }

    // Insert `value` at `key`, or overwrite if the key already exists.
    void insert(Key key, T value) {
        if (contains(key)) {
            values_[sparse_[key]] = std::move(value);
            return;
        }
        if (key >= sparse_.size()) sparse_.resize(static_cast<usize>(key) + 1, kInvalid);
        sparse_[key] = static_cast<u32>(dense_.size());
        dense_.push_back(key);
        values_.push_back(std::move(value));
    }

    // Remove `key` (swap-with-last). Returns false if it wasn't present.
    bool remove(Key key) {
        if (!contains(key)) return false;
        const u32 slot = sparse_[key];
        const u32 last = static_cast<u32>(dense_.size() - 1);
        const Key lastKey = dense_[last];

        dense_[slot] = lastKey;
        values_[slot] = std::move(values_[last]);
        sparse_[lastKey] = slot;

        dense_.pop_back();
        values_.pop_back();
        sparse_[key] = kInvalid;
        return true;
    }

    [[nodiscard]] T* get(Key key) noexcept {
        return contains(key) ? &values_[sparse_[key]] : nullptr;
    }
    [[nodiscard]] const T* get(Key key) const noexcept {
        return contains(key) ? &values_[sparse_[key]] : nullptr;
    }

    void clear() noexcept {
        sparse_.clear();
        dense_.clear();
        values_.clear();
    }

    // Dense value iteration (contiguous; order is unspecified after removals).
    [[nodiscard]] auto begin() noexcept { return values_.begin(); }
    [[nodiscard]] auto end() noexcept { return values_.end(); }
    [[nodiscard]] auto begin() const noexcept { return values_.begin(); }
    [[nodiscard]] auto end() const noexcept { return values_.end(); }

    // The keys, in the same order as the dense values.
    [[nodiscard]] const std::vector<Key>& keys() const noexcept { return dense_; }

private:
    std::vector<u32> sparse_;   // key -> index into dense_ (or kInvalid)
    std::vector<Key> dense_;    // packed list of live keys
    std::vector<T> values_;     // packed values, parallel to dense_
};

}  // namespace zukiru::containers
