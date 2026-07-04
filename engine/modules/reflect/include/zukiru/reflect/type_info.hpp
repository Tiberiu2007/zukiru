// Runtime type descriptions: a stable type identity, per-field metadata with
// type-erased accessors, and a TypeInfo record. This is the data the serializer
// and the editor inspector walk to read/write arbitrary structs generically.
#pragma once

#include <zukiru/core/types.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace zukiru::reflect {

// RTTI-free per-type identity (address of a function-local static is unique per
// instantiation of T). Works under -fno-rtti.
using TypeId = const void*;

template <class T>
[[nodiscard]] TypeId typeId() noexcept {
    static const char marker{};
    return &marker;
}

// One reflected member of a struct. Access is type-erased via a stored
// member pointer, so no offset arithmetic and no default-constructibility
// requirement on the owning type.
struct Field {
    std::string name;
    TypeId type = nullptr;  // typeId of the field's type
    usize size = 0;         // sizeof the field's type

    std::function<void*(void*)> access;
    std::function<const void*(const void*)> accessConst;

    // Pointer to this field within `instance`.
    [[nodiscard]] void* raw(void* instance) const { return access(instance); }
    [[nodiscard]] const void* raw(const void* instance) const { return accessConst(instance); }

    // Typed access. The caller is responsible for using the field's real type.
    template <class T>
    [[nodiscard]] const T& get(const void* instance) const {
        return *static_cast<const T*>(accessConst(instance));
    }
    template <class T>
    [[nodiscard]] T& getRef(void* instance) const {
        return *static_cast<T*>(access(instance));
    }
    template <class T>
    void set(void* instance, const T& value) const {
        *static_cast<T*>(access(instance)) = value;
    }

    template <class T>
    [[nodiscard]] bool is() const noexcept {
        return type == typeId<T>();
    }
};

// A described type: its name, identity, layout, and reflected fields.
struct TypeInfo {
    std::string name;
    TypeId id = nullptr;
    usize size = 0;
    usize alignment = 0;
    std::vector<Field> fields;

    [[nodiscard]] const Field* findField(std::string_view fieldName) const {
        for (const Field& field : fields) {
            if (field.name == fieldName) return &field;
        }
        return nullptr;
    }
};

}  // namespace zukiru::reflect
