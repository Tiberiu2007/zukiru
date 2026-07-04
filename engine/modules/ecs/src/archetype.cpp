#include <zukiru/ecs/archetype.hpp>

#include <new>
#include <utility>

namespace zukiru::ecs::detail {
namespace {

[[nodiscard]] std::byte* allocRaw(usize bytes, usize alignment) {
    if (bytes == 0) return nullptr;
    return static_cast<std::byte*>(::operator new(bytes, std::align_val_t{alignment}));
}

void freeRaw(std::byte* p, usize alignment) {
    if (p != nullptr) ::operator delete(p, std::align_val_t{alignment});
}

}  // namespace

// --- Column ---------------------------------------------------------------

Column::~Column() { freeRaw(data_, info_->alignment); }

Column::Column(Column&& other) noexcept
    : info_(other.info_), data_(other.data_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.capacity_ = 0;
}

void Column::reserve(usize newCapacity, usize liveCount) {
    if (newCapacity <= capacity_) return;
    std::byte* fresh = allocRaw(newCapacity * info_->size, info_->alignment);
    for (usize i = 0; i < liveCount; ++i) {
        void* src = data_ + i * info_->size;
        void* dst = fresh + i * info_->size;
        info_->moveConstruct(dst, src);
        info_->destruct(src);
    }
    freeRaw(data_, info_->alignment);
    data_ = fresh;
    capacity_ = newCapacity;
}

void Column::moveConstructInto(usize row, void* src) { info_->moveConstruct(elem(row), src); }

void Column::moveWithin(usize dst, usize src) { info_->moveConstruct(elem(dst), elem(src)); }

void Column::destroy(usize row) { info_->destruct(elem(row)); }

// --- Archetype ------------------------------------------------------------

Archetype::Archetype(std::vector<const ComponentInfo*> infos) : infos_(std::move(infos)) {
    columns_.reserve(infos_.size());
    for (const ComponentInfo* info : infos_) {
        columns_.emplace_back(*info);
    }
}

Archetype::~Archetype() {
    // Destruct every live element before the columns free their buffers.
    for (usize row = 0; row < count_; ++row) {
        for (Column& column : columns_) {
            column.destroy(row);
        }
    }
    count_ = 0;
}

usize Archetype::addRow(Entity e) {
    if (count_ == capacity_) {
        const usize newCapacity = capacity_ == 0 ? 4 : capacity_ * 2;
        for (Column& column : columns_) {
            column.reserve(newCapacity, count_);
        }
        capacity_ = newCapacity;
    }
    entities_.push_back(e);
    return count_++;
}

Entity Archetype::removeRow(usize row) {
    const usize last = count_ - 1;
    for (Column& column : columns_) {
        column.destroy(row);
        if (row != last) {
            column.moveWithin(row, last);  // relocate the last element into the hole
            column.destroy(last);
        }
    }

    Entity relocated = Entity::invalid();
    if (row != last) {
        entities_[row] = entities_[last];
        relocated = entities_[row];
    }
    entities_.pop_back();
    --count_;
    return relocated;
}

}  // namespace zukiru::ecs::detail
