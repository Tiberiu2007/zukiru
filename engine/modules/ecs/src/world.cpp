#include <zukiru/ecs/world.hpp>

#include <zukiru/core/assert.hpp>

#include <algorithm>

namespace zukiru::ecs {

using detail::Archetype;

World::World() { emptyArchetype_ = getOrCreateArchetype({}); }

World::~World() = default;

u32 World::componentTypeIndex(const ComponentInfo& info) {
    if (const auto it = componentIndex_.find(info.id); it != componentIndex_.end()) {
        return it->second;
    }
    const u32 index = static_cast<u32>(componentIndex_.size());
    componentIndex_.emplace(info.id, index);
    return index;
}

Archetype* World::getOrCreateArchetype(std::vector<const ComponentInfo*> infos) {
    // Canonical order: sort the component set by (stable) type index, so the same
    // set always yields the same signature regardless of insertion order.
    for (const ComponentInfo* info : infos) {
        (void)componentTypeIndex(*info);
    }
    std::sort(infos.begin(), infos.end(), [this](const ComponentInfo* a, const ComponentInfo* b) {
        return componentIndex_.at(a->id) < componentIndex_.at(b->id);
    });

    std::vector<u32> signature;
    signature.reserve(infos.size());
    for (const ComponentInfo* info : infos) {
        signature.push_back(componentIndex_.at(info->id));
    }

    if (const auto it = archetypeBySignature_.find(signature); it != archetypeBySignature_.end()) {
        return it->second;
    }

    auto owned = std::make_unique<Archetype>(std::move(infos));
    Archetype* ptr = owned.get();
    archetypes_.push_back(std::move(owned));
    archetypeBySignature_.emplace(std::move(signature), ptr);
    return ptr;
}

bool World::validate(Entity entity) const {
    return entity.index < records_.size() && records_[entity.index].alive &&
           records_[entity.index].generation == entity.generation;
}

Entity World::create() {
    u32 index = 0;
    if (!freeList_.empty()) {
        index = freeList_.back();
        freeList_.pop_back();
    } else {
        index = static_cast<u32>(records_.size());
        records_.push_back(EntityRecord{});
    }

    EntityRecord& rec = records_[index];
    rec.alive = true;
    rec.archetype = emptyArchetype_;
    const Entity e{index, rec.generation};
    rec.row = emptyArchetype_->addRow(e);
    ++aliveCount_;
    return e;
}

bool World::isAlive(Entity entity) const { return validate(entity); }

bool World::destroy(Entity entity) {
    if (!validate(entity)) return false;
    EntityRecord& rec = records_[entity.index];

    const Entity relocated = rec.archetype->removeRow(rec.row);
    if (relocated.isValid()) {
        records_[relocated.index].row = rec.row;
    }

    rec.alive = false;
    rec.archetype = nullptr;
    ++rec.generation;
    freeList_.push_back(entity.index);
    --aliveCount_;
    return true;
}

bool World::hasComponent(Entity entity, ComponentId id) const {
    return validate(entity) && records_[entity.index].archetype->has(id);
}

void* World::getComponent(Entity entity, ComponentId id) const {
    if (!validate(entity)) return nullptr;
    const EntityRecord& rec = records_[entity.index];
    return rec.archetype->componentAt(id, rec.row);
}

std::pair<void*, bool> World::addComponentRaw(Entity entity, const ComponentInfo& info) {
    ZUKIRU_ENSURE_MSG(validate(entity), "add() on a dead or invalid entity");
    EntityRecord& rec = records_[entity.index];
    Archetype* src = rec.archetype;

    // Already present: hand back the existing slot for assignment.
    if (src->has(info.id)) {
        return {src->componentAt(info.id, rec.row), true};
    }

    // Target archetype = current set + the new component.
    std::vector<const ComponentInfo*> infos = src->infos();
    infos.push_back(&info);
    Archetype* dst = getOrCreateArchetype(std::move(infos));

    const usize srcRow = rec.row;
    const usize dstRow = dst->addRow(entity);

    // Relocate the shared components; the new component's slot stays raw storage.
    for (const ComponentInfo* shared : src->infos()) {
        shared->moveConstruct(dst->componentAt(shared->id, dstRow),
                              src->componentAt(shared->id, srcRow));
    }

    const Entity relocated = src->removeRow(srcRow);
    if (relocated.isValid()) {
        records_[relocated.index].row = srcRow;
    }

    rec.archetype = dst;
    rec.row = dstRow;
    return {dst->componentAt(info.id, dstRow), false};
}

bool World::removeComponent(Entity entity, ComponentId id) {
    if (!validate(entity)) return false;
    EntityRecord& rec = records_[entity.index];
    Archetype* src = rec.archetype;
    if (!src->has(id)) return false;

    // Target archetype = current set minus the removed component.
    std::vector<const ComponentInfo*> infos;
    infos.reserve(src->infos().size() - 1);
    for (const ComponentInfo* info : src->infos()) {
        if (info->id != id) infos.push_back(info);
    }
    Archetype* dst = getOrCreateArchetype(std::move(infos));

    const usize srcRow = rec.row;
    const usize dstRow = dst->addRow(entity);

    // Relocate the kept components; the removed one is destructed by removeRow.
    for (const ComponentInfo* kept : dst->infos()) {
        kept->moveConstruct(dst->componentAt(kept->id, dstRow),
                            src->componentAt(kept->id, srcRow));
    }

    const Entity relocated = src->removeRow(srcRow);
    if (relocated.isValid()) {
        records_[relocated.index].row = srcRow;
    }

    rec.archetype = dst;
    rec.row = dstRow;
    return true;
}

}  // namespace zukiru::ecs
