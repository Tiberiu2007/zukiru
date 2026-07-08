#include <zuki/containers/sparse_set.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <vector>

using namespace zuki;
using namespace zuki::containers;

TEST_CASE("insert / contains / get", "[containers][sparse_set]") {
    SparseSet<int> set;
    REQUIRE(set.empty());
    set.insert(3, 30);
    set.insert(100, 1000);  // sparse array auto-grows
    REQUIRE(set.size() == 2);
    REQUIRE(set.contains(3));
    REQUIRE(set.contains(100));
    REQUIRE_FALSE(set.contains(4));
    REQUIRE(*set.get(3) == 30);
    REQUIRE(*set.get(100) == 1000);
    REQUIRE(set.get(4) == nullptr);
}

TEST_CASE("insert on existing key overwrites", "[containers][sparse_set]") {
    SparseSet<int> set;
    set.insert(5, 1);
    set.insert(5, 2);
    REQUIRE(set.size() == 1);
    REQUIRE(*set.get(5) == 2);
}

TEST_CASE("remove uses swap-with-last and keeps the rest intact",
          "[containers][sparse_set]") {
    SparseSet<int> set;
    for (u32 k = 0; k < 5; ++k) set.insert(k, static_cast<int>(k) * 10);
    REQUIRE(set.remove(1));
    REQUIRE_FALSE(set.remove(1));  // already gone
    REQUIRE(set.size() == 4);
    REQUIRE_FALSE(set.contains(1));
    // All other keys still map to the right values.
    for (u32 k : {0u, 2u, 3u, 4u}) {
        REQUIRE(set.contains(k));
        REQUIRE(*set.get(k) == static_cast<int>(k) * 10);
    }
}

TEST_CASE("dense iteration visits every value once", "[containers][sparse_set]") {
    SparseSet<int> set;
    for (u32 k = 0; k < 6; ++k) set.insert(k, static_cast<int>(k));
    set.remove(2);
    set.remove(4);

    std::vector<int> seen;
    for (int v : set) seen.push_back(v);
    std::sort(seen.begin(), seen.end());
    REQUIRE(seen == std::vector<int>{0, 1, 3, 5});
    REQUIRE(set.keys().size() == 4);
}

TEST_CASE("clear empties the set", "[containers][sparse_set]") {
    SparseSet<int> set;
    set.insert(1, 1);
    set.insert(2, 2);
    set.clear();
    REQUIRE(set.empty());
    REQUIRE_FALSE(set.contains(1));
    set.insert(1, 9);  // usable again
    REQUIRE(*set.get(1) == 9);
}
