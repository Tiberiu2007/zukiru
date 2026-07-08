#include <zuki/containers/slot_map.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <type_traits>
#include <vector>

using namespace zuki;
using namespace zuki::containers;

TEST_CASE("insert returns a valid handle to the value", "[containers][slot_map]") {
    SlotMap<std::string> map;
    const auto h = map.insert("hello");
    REQUIRE(map.size() == 1);
    REQUIRE(map.contains(h));
    REQUIRE(map.get(h) != nullptr);
    REQUIRE(*map.get(h) == "hello");
}

TEST_CASE("remove invalidates the handle", "[containers][slot_map]") {
    SlotMap<int> map;
    const auto h = map.insert(42);
    REQUIRE(map.remove(h));
    REQUIRE_FALSE(map.contains(h));
    REQUIRE(map.get(h) == nullptr);
    REQUIRE_FALSE(map.remove(h));  // double remove fails
    REQUIRE(map.empty());
}

TEST_CASE("a stale handle to a reused slot is rejected", "[containers][slot_map]") {
    SlotMap<int> map;
    const auto first = map.insert(1);
    REQUIRE(map.remove(first));
    const auto second = map.insert(2);  // reuses the same slot index

    REQUIRE(second.index == first.index);       // same slot...
    REQUIRE(second.generation != first.generation);  // ...but newer generation
    REQUIRE_FALSE(map.contains(first));         // stale handle rejected
    REQUIRE(map.contains(second));
    REQUIRE(*map.get(second) == 2);
}

TEST_CASE("many handles stay valid as the map grows", "[containers][slot_map]") {
    SlotMap<int> map;
    std::vector<SlotMap<int>::Handle> handles;
    for (int i = 0; i < 100; ++i) handles.push_back(map.insert(i));
    REQUIRE(map.size() == 100);
    for (int i = 0; i < 100; ++i) {
        REQUIRE(map.contains(handles[static_cast<usize>(i)]));
        REQUIRE(*map.get(handles[static_cast<usize>(i)]) == i);
    }
}

TEST_CASE("forEach visits only live elements", "[containers][slot_map]") {
    SlotMap<int> map;
    const auto a = map.insert(10);
    const auto b = map.insert(20);
    const auto c = map.insert(30);
    map.remove(b);

    int sum = 0;
    int visits = 0;
    map.forEach([&](SlotMap<int>::Handle, int& value) {
        sum += value;
        ++visits;
    });
    REQUIRE(visits == 2);
    REQUIRE(sum == 40);
    (void)a;
    (void)c;
}

TEST_CASE("clear removes all and invalidates handles", "[containers][slot_map]") {
    SlotMap<int> map;
    const auto h = map.insert(5);
    map.clear();
    REQUIRE(map.empty());
    REQUIRE_FALSE(map.contains(h));
}

TEST_CASE("distinct tags produce distinct handle types", "[containers][slot_map]") {
    struct A;
    struct B;
    STATIC_REQUIRE_FALSE(
        std::is_same_v<SlotMap<int, A>::Handle, SlotMap<int, B>::Handle>);
}
