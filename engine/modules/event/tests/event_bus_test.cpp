#include <zuki/event/event_bus.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace zuki;
using namespace zuki::event;

namespace {
struct Damage {
    i32 amount;
};
struct Healed {
    i32 amount;
};
}  // namespace

TEST_CASE("publish delivers the event to a subscriber", "[event]") {
    EventBus bus;
    i32 received = 0;
    auto sub = bus.subscribe<Damage>([&received](const Damage& e) { received = e.amount; });
    bus.publish(Damage{25});
    REQUIRE(received == 25);
}

TEST_CASE("all subscribers of a type are invoked", "[event]") {
    EventBus bus;
    i32 total = 0;
    auto a = bus.subscribe<Damage>([&total](const Damage& e) { total += e.amount; });
    auto b = bus.subscribe<Damage>([&total](const Damage& e) { total += e.amount * 2; });
    REQUIRE(bus.handlerCount<Damage>() == 2);
    bus.publish(Damage{10});
    REQUIRE(total == 30);  // 10 + 20
}

TEST_CASE("event types are isolated from each other", "[event]") {
    EventBus bus;
    i32 damage = 0;
    i32 heal = 0;
    auto d = bus.subscribe<Damage>([&damage](const Damage& e) { damage += e.amount; });
    auto h = bus.subscribe<Healed>([&heal](const Healed& e) { heal += e.amount; });
    bus.publish(Damage{7});
    REQUIRE(damage == 7);
    REQUIRE(heal == 0);
}

TEST_CASE("a destroyed Subscription stops delivery (RAII)", "[event]") {
    EventBus bus;
    i32 count = 0;
    {
        auto sub = bus.subscribe<Damage>([&count](const Damage&) { ++count; });
        bus.publish(Damage{1});
        REQUIRE(count == 1);
    }  // sub goes out of scope -> unsubscribed
    REQUIRE(bus.handlerCount<Damage>() == 0);
    bus.publish(Damage{1});
    REQUIRE(count == 1);  // not incremented again
}

TEST_CASE("explicit unsubscribe removes the handler", "[event]") {
    EventBus bus;
    i32 count = 0;
    auto sub = bus.subscribe<Damage>([&count](const Damage&) { ++count; });
    sub.unsubscribe();
    REQUIRE_FALSE(sub.active());
    bus.publish(Damage{1});
    REQUIRE(count == 0);
}

TEST_CASE("detach keeps the handler alive past the token", "[event]") {
    EventBus bus;
    i32 count = 0;
    {
        auto sub = bus.subscribe<Damage>([&count](const Damage&) { ++count; });
        sub.detach();
    }
    bus.publish(Damage{1});
    REQUIRE(count == 1);  // still delivered
    REQUIRE(bus.handlerCount<Damage>() == 1);
}

TEST_CASE("enqueue defers delivery until dispatchQueued", "[event]") {
    EventBus bus;
    std::vector<i32> order;
    auto sub = bus.subscribe<Damage>([&order](const Damage& e) { order.push_back(e.amount); });

    bus.enqueue(Damage{1});
    bus.enqueue(Damage{2});
    bus.enqueue(Damage{3});
    REQUIRE(bus.queuedCount() == 3);
    REQUIRE(order.empty());  // nothing delivered yet

    bus.dispatchQueued();
    REQUIRE(order == std::vector<i32>{1, 2, 3});  // in order
    REQUIRE(bus.queuedCount() == 0);
}

TEST_CASE("publishing with no subscribers is a no-op", "[event]") {
    EventBus bus;
    REQUIRE_NOTHROW(bus.publish(Damage{99}));
    REQUIRE(bus.handlerCount<Damage>() == 0);
}

TEST_CASE("unsubscribing from within a handler is safe", "[event]") {
    EventBus bus;
    i32 count = 0;
    Subscription self;
    self = bus.subscribe<Damage>([&](const Damage&) {
        ++count;
        self.unsubscribe();  // remove myself mid-dispatch
    });
    bus.publish(Damage{1});  // snapshot dispatch: this call still reaches us once
    REQUIRE(count == 1);
    bus.publish(Damage{1});  // now gone
    REQUIRE(count == 1);
}

TEST_CASE("clear removes all handlers and queued events", "[event]") {
    EventBus bus;
    auto sub = bus.subscribe<Damage>([](const Damage&) {});
    bus.enqueue(Damage{1});
    bus.clear();
    REQUIRE(bus.handlerCount<Damage>() == 0);
    REQUIRE(bus.queuedCount() == 0);
    sub.detach();  // avoid touching the cleared bus on destruction
}
