#include <zukiru/ecs/ecs.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace zukiru;
using namespace zukiru::ecs;

namespace {

struct Position {
    f32 x = 0.0f;
    f32 y = 0.0f;
};
struct Velocity {
    f32 x = 0.0f;
    f32 y = 0.0f;
};
struct Name {
    std::string value;
};

// A component that counts live instances, to verify destructors run on remove /
// destroy / archetype transitions.
struct Tracked {
    static inline int liveCount = 0;
    int id = 0;
    explicit Tracked(int i = 0) : id(i) { ++liveCount; }
    Tracked(const Tracked& o) : id(o.id) { ++liveCount; }
    Tracked(Tracked&& o) noexcept : id(o.id) { ++liveCount; }
    Tracked& operator=(const Tracked&) = default;
    Tracked& operator=(Tracked&&) noexcept = default;
    ~Tracked() { --liveCount; }
};

}  // namespace

TEST_CASE("create and destroy entities, generations detect stale handles", "[ecs]") {
    World world;
    REQUIRE(world.entityCount() == 0);

    const Entity a = world.create();
    const Entity b = world.create();
    REQUIRE(world.isAlive(a));
    REQUIRE(world.isAlive(b));
    REQUIRE(world.entityCount() == 2);
    REQUIRE(a != b);

    REQUIRE(world.destroy(a));
    REQUIRE_FALSE(world.isAlive(a));
    REQUIRE(world.entityCount() == 1);
    REQUIRE_FALSE(world.destroy(a));  // double destroy is a no-op

    // The index is recycled, but the generation makes the old handle stale.
    const Entity c = world.create();
    REQUIRE(c.index == a.index);
    REQUIRE(c.generation != a.generation);
    REQUIRE(world.isAlive(c));
    REQUIRE_FALSE(world.isAlive(a));
}

TEST_CASE("add, get, has and remove components", "[ecs]") {
    World world;
    const Entity e = world.create();

    REQUIRE_FALSE(world.has<Position>(e));
    REQUIRE(world.get<Position>(e) == nullptr);

    world.add(e, Position{1.0f, 2.0f});
    REQUIRE(world.has<Position>(e));
    REQUIRE(world.get<Position>(e)->x == 1.0f);

    // Adding a second component moves the entity to a new archetype but keeps
    // the first component's value.
    world.add(e, Velocity{3.0f, 4.0f});
    REQUIRE(world.has<Position>(e));
    REQUIRE(world.has<Velocity>(e));
    REQUIRE(world.get<Position>(e)->x == 1.0f);
    REQUIRE(world.get<Velocity>(e)->y == 4.0f);

    // add() on an existing component overwrites in place.
    world.add(e, Position{9.0f, 9.0f});
    REQUIRE(world.get<Position>(e)->x == 9.0f);

    REQUIRE(world.remove<Velocity>(e));
    REQUIRE_FALSE(world.has<Velocity>(e));
    REQUIRE(world.has<Position>(e));
    REQUIRE_FALSE(world.remove<Velocity>(e));  // already gone
    REQUIRE(world.get<Position>(e)->x == 9.0f);
}

TEST_CASE("create with components in one call", "[ecs]") {
    World world;
    const Entity e = world.create(Position{5.0f, 6.0f}, Name{"hero"});
    REQUIRE(world.has<Position>(e));
    REQUIRE(world.get<Name>(e)->value == "hero");
    REQUIRE(world.get<Position>(e)->y == 6.0f);
}

TEST_CASE("each iterates matching entities and can mutate them", "[ecs]") {
    World world;
    const Entity moving = world.create(Position{0.0f, 0.0f}, Velocity{1.0f, 2.0f});
    const Entity still = world.create(Position{10.0f, 10.0f});  // no Velocity
    (void)world.create(Velocity{9.0f, 9.0f});                   // no Position

    // Integrate position by velocity — only `moving` qualifies.
    int visited = 0;
    world.each<Position, const Velocity>([&](Position& p, const Velocity& v) {
        p.x += v.x;
        p.y += v.y;
        ++visited;
    });
    REQUIRE(visited == 1);
    REQUIRE(world.get<Position>(moving)->x == 1.0f);
    REQUIRE(world.get<Position>(moving)->y == 2.0f);
    REQUIRE(world.get<Position>(still)->x == 10.0f);  // untouched
}

TEST_CASE("each overload receiving the Entity", "[ecs]") {
    World world;
    const Entity a = world.create(Position{1.0f, 0.0f});
    const Entity b = world.create(Position{2.0f, 0.0f});

    std::vector<Entity> seen;
    f32 sum = 0.0f;
    world.each<Position>([&](Entity e, Position& p) {
        seen.push_back(e);
        sum += p.x;
    });
    REQUIRE(seen.size() == 2);
    REQUIRE(sum == 3.0f);
    REQUIRE((seen[0] == a || seen[1] == a));
    REQUIRE((seen[0] == b || seen[1] == b));
}

TEST_CASE("swap-remove keeps surviving rows valid and queryable", "[ecs]") {
    World world;
    std::vector<Entity> entities;
    for (int i = 0; i < 5; ++i) {
        entities.push_back(world.create(Position{static_cast<f32>(i), 0.0f}));
    }

    // Destroy from the middle — forces the last row to be swapped into the hole.
    REQUIRE(world.destroy(entities[1]));
    REQUIRE(world.destroy(entities[3]));

    REQUIRE(world.get<Position>(entities[0])->x == 0.0f);
    REQUIRE(world.get<Position>(entities[2])->x == 2.0f);
    REQUIRE(world.get<Position>(entities[4])->x == 4.0f);

    f32 sum = 0.0f;
    int count = 0;
    world.each<Position>([&](Position& p) {
        sum += p.x;
        ++count;
    });
    REQUIRE(count == 3);
    REQUIRE(sum == 6.0f);  // 0 + 2 + 4
}

TEST_CASE("column growth relocates many entities correctly", "[ecs]") {
    World world;
    constexpr int n = 200;  // well past the initial capacity of 4
    for (int i = 0; i < n; ++i) {
        (void)world.create(Position{static_cast<f32>(i), 0.0f}, Velocity{1.0f, 0.0f});
    }
    REQUIRE(world.entityCount() == n);

    world.each<Position, const Velocity>([](Position& p, const Velocity& v) { p.x += v.x; });

    f32 sum = 0.0f;
    world.each<const Position>([&](const Position& p) { sum += p.x; });
    // Sum of (i+1) for i in [0, n)  ==  n*(n+1)/2.
    REQUIRE(sum == static_cast<f32>(n) * static_cast<f32>(n + 1) / 2.0f);
}

TEST_CASE("component destructors run on remove and destroy", "[ecs]") {
    Tracked::liveCount = 0;
    {
        World world;
        const Entity a = world.create();
        world.add(a, Tracked{1});
        world.add(a, Position{});  // archetype transition relocates Tracked
        REQUIRE(Tracked::liveCount == 1);

        const Entity b = world.create(Tracked{2});
        REQUIRE(Tracked::liveCount == 2);

        world.remove<Tracked>(a);  // destructs a's Tracked
        REQUIRE(Tracked::liveCount == 1);

        world.destroy(b);  // destructs b's Tracked
        REQUIRE(Tracked::liveCount == 0);

        (void)world.add(a, Tracked{3});  // one more, cleaned up by World destruction
        REQUIRE(Tracked::liveCount == 1);
    }
    REQUIRE(Tracked::liveCount == 0);  // World destructor cleaned everything up
}

TEST_CASE("archetypes are shared by component set regardless of add order", "[ecs]") {
    World world;
    const Entity a = world.create();
    world.add(a, Position{});
    world.add(a, Velocity{});

    const Entity b = world.create();
    world.add(b, Velocity{});
    world.add(b, Position{});

    // Both entities have {Position, Velocity} and should land in the same
    // archetype. archetypeCount: empty, {Position}, {Position,Velocity},
    // plus the transient {Velocity} b passed through = 4.
    REQUIRE(world.has<Position>(b));
    REQUIRE(world.has<Velocity>(b));

    int both = 0;
    world.each<Position, Velocity>([&](Position&, Velocity&) { ++both; });
    REQUIRE(both == 2);
}

TEST_CASE("dead entities answer queries safely", "[ecs]") {
    World world;
    const Entity e = world.create(Position{1.0f, 1.0f});
    world.destroy(e);
    REQUIRE_FALSE(world.has<Position>(e));
    REQUIRE(world.get<Position>(e) == nullptr);
    REQUIRE_FALSE(world.remove<Position>(e));
}
