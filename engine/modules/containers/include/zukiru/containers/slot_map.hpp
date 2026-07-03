// SlotMap<T, Tag> — stable, generational handles to values.
//
// insert() returns a Handle (index + generation). The slot can be reused after
// remove(), but its generation is bumped, so a handle to the old occupant is
// detected as stale and get()/contains() reject it — safe weak references with
// no dangling pointers. Values live in a slab that reuses freed slots via a
// free list, so handles stay valid even as the map grows.
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/memory/handle.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace zukiru::containers {

// Default phantom tag so `SlotMap<T>` just works; pass your own for type-safety
// across different resource kinds.
struct SlotMapDefaultTag;

template <class T, class Tag = SlotMapDefaultTag>
class SlotMap {
public:
    using Handle = memory::Handle<Tag>;

    [[nodiscard]] usize size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    // Insert a value; returns a handle that stays valid until the value is removed.
    [[nodiscard]] Handle insert(T value) {
        u32 index = 0;
        if (!freeList_.empty()) {
            index = freeList_.back();
            freeList_.pop_back();
            items_[index] = std::move(value);
        } else {
            index = static_cast<u32>(items_.size());
            items_.emplace_back(std::move(value));
            generations_.push_back(0);
        }
        ++size_;
        return Handle{index, generations_[index]};
    }

    [[nodiscard]] bool contains(Handle handle) const noexcept {
        return handle.index < items_.size() && generations_[handle.index] == handle.generation &&
               items_[handle.index].has_value();
    }

    [[nodiscard]] T* get(Handle handle) noexcept {
        return contains(handle) ? &*items_[handle.index] : nullptr;
    }
    [[nodiscard]] const T* get(Handle handle) const noexcept {
        return contains(handle) ? &*items_[handle.index] : nullptr;
    }

    // Remove the value a handle refers to. Returns false if the handle is stale.
    bool remove(Handle handle) {
        if (!contains(handle)) return false;
        items_[handle.index].reset();
        ++generations_[handle.index];  // invalidate outstanding handles to this slot
        freeList_.push_back(handle.index);
        --size_;
        return true;
    }

    void clear() noexcept {
        for (usize i = 0; i < items_.size(); ++i) {
            if (items_[i].has_value()) {
                items_[i].reset();
                ++generations_[i];
            }
        }
        freeList_.clear();
        for (u32 i = 0; i < static_cast<u32>(items_.size()); ++i) freeList_.push_back(i);
        size_ = 0;
    }

    // Visit every live element: fn(Handle, T&).
    template <class Fn>
    void forEach(Fn&& fn) {
        for (usize i = 0; i < items_.size(); ++i) {
            if (items_[i].has_value()) {
                fn(Handle{static_cast<u32>(i), generations_[i]}, *items_[i]);
            }
        }
    }
    template <class Fn>
    void forEach(Fn&& fn) const {
        for (usize i = 0; i < items_.size(); ++i) {
            if (items_[i].has_value()) {
                fn(Handle{static_cast<u32>(i), generations_[i]}, *items_[i]);
            }
        }
    }

private:
    std::vector<std::optional<T>> items_;
    std::vector<u32> generations_;
    std::vector<u32> freeList_;
    usize size_ = 0;
};

}  // namespace zukiru::containers
