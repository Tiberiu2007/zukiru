// The type registry and its fluent builder.
//
//   struct Transform { Vec3 position; Quat rotation; f32 scale; };
//
//   registry.registerType<Transform>("Transform")
//       .field("position", &Transform::position)
//       .field("rotation", &Transform::rotation)
//       .field("scale",    &Transform::scale);
//
//   const TypeInfo* info = registry.find<Transform>();
//   for (const Field& f : info->fields) { ... }   // generic walk
#pragma once

#include <zuki/core/types.hpp>
#include <zuki/reflect/type_info.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace zuki::reflect {

// Fluent helper for adding fields to a type being registered.
template <class T>
class TypeBuilder {
public:
    explicit TypeBuilder(TypeInfo* info) noexcept : info_(info) {}

    template <class M>
    TypeBuilder& field(std::string name, M T::* member) {
        Field field;
        field.name = std::move(name);
        field.type = typeId<M>();
        field.size = sizeof(M);
        field.access = [member](void* instance) -> void* {
            return std::addressof(static_cast<T*>(instance)->*member);
        };
        field.accessConst = [member](const void* instance) -> const void* {
            return std::addressof(static_cast<const T*>(instance)->*member);
        };
        info_->fields.push_back(std::move(field));
        return *this;
    }

    [[nodiscard]] const TypeInfo& info() const noexcept { return *info_; }

private:
    TypeInfo* info_;
};

class Registry {
public:
    Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    // Movable: TypeInfo lives on the heap, so pointers into it stay valid.
    Registry(Registry&&) = default;
    Registry& operator=(Registry&&) = default;

    // Begin describing type T under `name`. Re-registering a type replaces it.
    template <class T>
    TypeBuilder<T> registerType(std::string name) {
        auto info = std::make_unique<TypeInfo>();
        info->name = name;
        info->id = typeId<T>();
        info->size = sizeof(T);
        info->alignment = alignof(T);

        TypeInfo* raw = info.get();
        byName_[name] = raw;
        byId_[typeId<T>()] = std::move(info);
        return TypeBuilder<T>(raw);
    }

    [[nodiscard]] const TypeInfo* find(TypeId id) const {
        const auto it = byId_.find(id);
        return it == byId_.end() ? nullptr : it->second.get();
    }
    [[nodiscard]] const TypeInfo* find(std::string_view name) const {
        const auto it = byName_.find(std::string{name});
        return it == byName_.end() ? nullptr : it->second;
    }
    // Disambiguates string literals: without this, `find("X")` would bind to the
    // TypeId (const void*) overload via pointer conversion instead of by-name.
    [[nodiscard]] const TypeInfo* find(const char* name) const {
        return find(std::string_view{name});
    }
    template <class T>
    [[nodiscard]] const TypeInfo* find() const {
        return find(typeId<T>());
    }

    template <class T>
    [[nodiscard]] bool isRegistered() const {
        return byId_.contains(typeId<T>());
    }

    [[nodiscard]] usize typeCount() const noexcept { return byId_.size(); }

    // Process-wide registry (engine-default). Tests can also use local instances.
    [[nodiscard]] static Registry& global() {
        static Registry instance;
        return instance;
    }

private:
    std::unordered_map<TypeId, std::unique_ptr<TypeInfo>> byId_;
    std::unordered_map<std::string, TypeInfo*> byName_;
};

}  // namespace zuki::reflect
