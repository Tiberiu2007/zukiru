// Archetype — storage for all entities that have exactly one given set of
// component types, laid out struct-of-arrays: one contiguous Column per
// component, plus a parallel array of the entities occupying each row.
//
// This is an internal building block; game code uses World. Rows are packed:
// removing a row swaps in the last one (O(1)), which is why removeRow reports the
// entity it relocated so the World can fix that entity's recorded row.
#pragma once

#include <zuki/core/types.hpp>
#include <zuki/ecs/component.hpp>
#include <zuki/ecs/entity.hpp>

#include <vector>

namespace zuki::ecs::detail {

// One component's contiguous storage. Element count lives in the owning
// Archetype; Column just holds the (aligned) buffer and its capacity.
class Column {
public:
    explicit Column(const ComponentInfo& info) noexcept : info_(&info) {}
    ~Column();

    Column(Column&& other) noexcept;
    Column& operator=(Column&&) = delete;
    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;

    [[nodiscard]] const ComponentInfo& info() const noexcept { return *info_; }
    [[nodiscard]] void* elem(usize row) const noexcept { return data_ + row * info_->size; }

    // Grow to hold at least `newCapacity` elements, relocating `liveCount` of
    // them from the old buffer.
    void reserve(usize newCapacity, usize liveCount);
    // Move-construct the element at `row` from `src` (row was raw storage).
    void moveConstructInto(usize row, void* src);
    // Move-construct `dst` from `src` within this column (dst was raw storage).
    void moveWithin(usize dst, usize src);
    // Destruct the element at `row`.
    void destroy(usize row);

private:
    const ComponentInfo* info_;
    std::byte* data_ = nullptr;
    usize capacity_ = 0;
};

class Archetype {
public:
    // `infos` are the component types of this archetype (order defines column
    // order; the World passes them in a canonical, signature-sorted order).
    explicit Archetype(std::vector<const ComponentInfo*> infos);
    ~Archetype();

    Archetype(const Archetype&) = delete;
    Archetype& operator=(const Archetype&) = delete;

    [[nodiscard]] const std::vector<const ComponentInfo*>& infos() const noexcept { return infos_; }
    [[nodiscard]] usize count() const noexcept { return count_; }
    [[nodiscard]] Entity entityAt(usize row) const noexcept { return entities_[row]; }

    [[nodiscard]] bool has(ComponentId id) const noexcept {
        for (const ComponentInfo* info : infos_) {
            if (info->id == id) return true;
        }
        return false;
    }
    // Base pointer of the column for `id` (index it by row), or nullptr.
    [[nodiscard]] void* column(ComponentId id) const noexcept {
        for (usize i = 0; i < infos_.size(); ++i) {
            if (infos_[i]->id == id) return columns_[i].elem(0);
        }
        return nullptr;
    }
    // Address of component `id` for the entity at `row`, or nullptr.
    [[nodiscard]] void* componentAt(ComponentId id, usize row) const noexcept {
        for (usize i = 0; i < infos_.size(); ++i) {
            if (infos_[i]->id == id) return columns_[i].elem(row);
        }
        return nullptr;
    }

    // Append a row for `e` (components left as raw storage for the caller to
    // fill). Returns the new row index.
    usize addRow(Entity e);
    // Remove `row` (destructing its components), swapping in the last row.
    // Returns the entity relocated into `row`, or an invalid Entity if `row` was
    // already last.
    Entity removeRow(usize row);

private:
    std::vector<const ComponentInfo*> infos_;
    std::vector<Column> columns_;
    std::vector<Entity> entities_;
    usize count_ = 0;
    usize capacity_ = 0;
};

}  // namespace zuki::ecs::detail
