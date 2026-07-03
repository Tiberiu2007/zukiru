#include <zukiru/containers/ring_buffer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zukiru;
using namespace zukiru::containers;

TEST_CASE("push / pop is FIFO", "[containers][ring_buffer]") {
    RingBuffer<int> ring(4);
    REQUIRE(ring.empty());
    REQUIRE(ring.capacity() == 4);
    REQUIRE(ring.push(1));
    REQUIRE(ring.push(2));
    REQUIRE(ring.push(3));
    REQUIRE(ring.size() == 3);

    REQUIRE(*ring.front() == 1);
    REQUIRE(*ring.back() == 3);
    REQUIRE(ring.pop() == 1);
    REQUIRE(ring.pop() == 2);
    REQUIRE(ring.size() == 1);
}

TEST_CASE("push fails when full; pop returns nullopt when empty",
          "[containers][ring_buffer]") {
    RingBuffer<int> ring(2);
    REQUIRE(ring.push(1));
    REQUIRE(ring.push(2));
    REQUIRE(ring.full());
    REQUIRE_FALSE(ring.push(3));  // rejected, buffer unchanged
    REQUIRE(ring.size() == 2);

    REQUIRE(ring.pop() == 1);
    REQUIRE(ring.pop() == 2);
    REQUIRE_FALSE(ring.pop().has_value());
}

TEST_CASE("pushOverwrite drops the oldest element", "[containers][ring_buffer]") {
    RingBuffer<int> ring(3);
    ring.pushOverwrite(1);
    ring.pushOverwrite(2);
    ring.pushOverwrite(3);
    ring.pushOverwrite(4);  // overwrites 1
    REQUIRE(ring.full());
    REQUIRE(*ring.front() == 2);
    REQUIRE(*ring.back() == 4);
    REQUIRE(ring.pop() == 2);
    REQUIRE(ring.pop() == 3);
    REQUIRE(ring.pop() == 4);
}

TEST_CASE("wrap-around reuses slots correctly", "[containers][ring_buffer]") {
    RingBuffer<int> ring(3);
    REQUIRE(ring.push(1));
    REQUIRE(ring.push(2));
    REQUIRE(ring.pop() == 1);          // head advances
    REQUIRE(ring.push(3));
    REQUIRE(ring.push(4));             // wraps into the freed slot
    REQUIRE(ring.full());
    REQUIRE(ring.pop() == 2);
    REQUIRE(ring.pop() == 3);
    REQUIRE(ring.pop() == 4);
    REQUIRE(ring.empty());
}

TEST_CASE("clear empties the buffer", "[containers][ring_buffer]") {
    RingBuffer<int> ring(4);
    ring.push(1);
    ring.push(2);
    ring.clear();
    REQUIRE(ring.empty());
    REQUIRE(ring.front() == nullptr);
    REQUIRE(ring.push(9));
    REQUIRE(*ring.front() == 9);
}
