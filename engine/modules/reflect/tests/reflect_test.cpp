#include <zuki/reflect/reflect.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace zuki;
using namespace zuki::reflect;

namespace {

struct Vec3 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
};

struct Entity {
    i32 id = 0;
    f32 health = 100.0f;
    bool alive = true;
    Vec3 position;
};

// Build a registry describing the types above.
Registry makeRegistry() {
    Registry registry;
    registry.registerType<Vec3>("Vec3")
        .field("x", &Vec3::x)
        .field("y", &Vec3::y)
        .field("z", &Vec3::z);
    registry.registerType<Entity>("Entity")
        .field("id", &Entity::id)
        .field("health", &Entity::health)
        .field("alive", &Entity::alive)
        .field("position", &Entity::position);
    return registry;
}

}  // namespace

TEST_CASE("registered types are found by T, id and name", "[reflect]") {
    const Registry registry = makeRegistry();
    REQUIRE(registry.typeCount() == 2);

    const TypeInfo* byT = registry.find<Entity>();
    REQUIRE(byT != nullptr);
    REQUIRE(byT->name == "Entity");
    REQUIRE(byT->size == sizeof(Entity));
    REQUIRE(byT->alignment == alignof(Entity));

    REQUIRE(registry.find(typeId<Entity>()) == byT);
    REQUIRE(registry.find("Entity") == byT);
    REQUIRE(registry.find("Nope") == nullptr);
    REQUIRE(registry.isRegistered<Vec3>());
}

TEST_CASE("fields expose names and types in order", "[reflect]") {
    const Registry registry = makeRegistry();
    const TypeInfo* info = registry.find<Entity>();
    REQUIRE(info->fields.size() == 4);
    REQUIRE(info->fields[0].name == "id");
    REQUIRE(info->fields[0].is<i32>());
    REQUIRE(info->fields[1].name == "health");
    REQUIRE(info->fields[1].is<f32>());
    REQUIRE(info->fields[3].is<Vec3>());
}

TEST_CASE("findField locates a field by name", "[reflect]") {
    const Registry registry = makeRegistry();
    const TypeInfo* info = registry.find<Entity>();
    REQUIRE(info->findField("health") != nullptr);
    REQUIRE(info->findField("health")->is<f32>());
    REQUIRE(info->findField("missing") == nullptr);
}

TEST_CASE("get / set field values on an instance", "[reflect]") {
    const Registry registry = makeRegistry();
    const TypeInfo* info = registry.find<Entity>();
    Entity e;

    const Field* health = info->findField("health");
    REQUIRE(health->get<f32>(&e) == 100.0f);
    health->set<f32>(&e, 42.0f);
    REQUIRE(e.health == 42.0f);

    info->findField("alive")->set<bool>(&e, false);
    REQUIRE_FALSE(e.alive);

    // Mutating through getRef.
    info->findField("id")->getRef<i32>(&e) = 7;
    REQUIRE(e.id == 7);
}

TEST_CASE("nested struct fields resolve through the registry", "[reflect]") {
    const Registry registry = makeRegistry();
    const TypeInfo* entityInfo = registry.find<Entity>();

    const Field* positionField = entityInfo->findField("position");
    REQUIRE(positionField->is<Vec3>());

    // Resolve the field's own type description and walk into it.
    const TypeInfo* vecInfo = registry.find(positionField->type);
    REQUIRE(vecInfo != nullptr);
    REQUIRE(vecInfo->name == "Vec3");
    REQUIRE(vecInfo->fields.size() == 3);

    Entity e;
    void* positionPtr = positionField->raw(&e);
    vecInfo->findField("y")->set<f32>(positionPtr, 9.0f);
    REQUIRE(e.position.y == 9.0f);
}

TEST_CASE("generic reflection-driven serialization", "[reflect]") {
    const Registry registry = makeRegistry();
    const TypeInfo* info = registry.find<Entity>();

    Entity e;
    e.id = 3;
    e.health = 55.0f;
    e.alive = true;

    // Walk fields, emitting "name=value;" for the primitive ones.
    std::string out;
    for (const Field& field : info->fields) {
        if (field.is<i32>()) {
            out += field.name + "=" + std::to_string(field.get<i32>(&e)) + ";";
        } else if (field.is<f32>()) {
            out += field.name + "=" + std::to_string(field.get<f32>(&e)) + ";";
        } else if (field.is<bool>()) {
            out += field.name + "=" + (field.get<bool>(&e) ? "true" : "false") + ";";
        }
    }
    REQUIRE(out == "id=3;health=55.000000;alive=true;");
}

TEST_CASE("global registry is a single shared instance", "[reflect]") {
    REQUIRE(&Registry::global() == &Registry::global());
}
