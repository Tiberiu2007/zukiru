// EventBus — a type-safe publish/subscribe message dispatcher.
//
// Events are plain user types (no base class required). Handlers subscribe to a
// specific event type and are invoked when a value of that type is published.
//
//   struct DamageTaken { EntityId who; f32 amount; };
//
//   EventBus bus;
//   auto sub = bus.subscribe<DamageTaken>([](const DamageTaken& e){ ... });
//   bus.publish(DamageTaken{hero, 10.0f});   // synchronous dispatch
//
//   bus.enqueue(DamageTaken{hero, 5.0f});     // deferred...
//   bus.dispatchQueued();                     // ...flushed here (e.g. once per frame)
//
// Threading: an EventBus is NOT synchronized; use one per thread or add external
// locking. Subscriptions must not outlive their bus.
#pragma once

#include <zukiru/core/types.hpp>

#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zukiru::event {

namespace detail {
// A cheap, RTTI-free per-type identity: the address of a function-local static
// is unique per instantiation of T.
using TypeId = const void*;
template <class T>
[[nodiscard]] TypeId typeId() noexcept {
    static const char marker{};
    return &marker;
}
}  // namespace detail

class EventBus;

// RAII token for a subscription. Removes its handler on destruction unless
// detach()ed. Move-only. [[nodiscard]] on subscribe() guards against accidentally
// dropping the token (which would unsubscribe immediately).
class Subscription {
public:
    Subscription() = default;
    Subscription(EventBus* bus, detail::TypeId type, u64 id) noexcept
        : bus_(bus), type_(type), id_(id) {}

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : bus_(other.bus_), type_(other.type_), id_(other.id_) {
        other.bus_ = nullptr;
    }
    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            unsubscribe();
            bus_ = other.bus_;
            type_ = other.type_;
            id_ = other.id_;
            other.bus_ = nullptr;
        }
        return *this;
    }

    ~Subscription() { unsubscribe(); }

    // Remove the handler now.
    void unsubscribe() noexcept;

    // Stop managing the handler: it stays subscribed for the life of the bus.
    void detach() noexcept { bus_ = nullptr; }

    [[nodiscard]] bool active() const noexcept { return bus_ != nullptr; }

private:
    EventBus* bus_ = nullptr;
    detail::TypeId type_ = nullptr;
    u64 id_ = 0;
};

class EventBus {
public:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;  // Subscriptions hold a raw pointer to us.
    EventBus& operator=(EventBus&&) = delete;

    // Register a handler `void(const E&)` for event type E.
    template <class E, class Fn>
    [[nodiscard]] Subscription subscribe(Fn&& handler) {
        const u64 id = nextId_++;
        auto& list = handlers_[detail::typeId<E>()];
        list.push_back(Handler{id, [fn = std::forward<Fn>(handler)](const void* payload) {
                                   fn(*static_cast<const E*>(payload));
                               }});
        return Subscription{this, detail::typeId<E>(), id};
    }

    // Synchronously dispatch `event` to every handler of its type.
    template <class E>
    void publish(const E& event) {
        const auto it = handlers_.find(detail::typeId<E>());
        if (it == handlers_.end()) return;
        // Snapshot so (un)subscribing from within a handler is safe: it affects
        // future dispatches, not this one.
        const std::vector<Handler> snapshot = it->second;
        for (const Handler& handler : snapshot) handler.fn(&event);
    }

    // Store an event for later; delivered in order on dispatchQueued().
    template <class E>
    void enqueue(E event) {
        queued_.push_back([this, ev = std::move(event)] { publish(ev); });
    }

    // Deliver all queued events (in enqueue order). Events enqueued during
    // dispatch are held for the next call rather than processed reentrantly.
    void dispatchQueued() {
        std::vector<std::function<void()>> batch;
        batch.swap(queued_);
        for (auto& deliver : batch) deliver();
    }

    template <class E>
    [[nodiscard]] usize handlerCount() const {
        const auto it = handlers_.find(detail::typeId<E>());
        return it == handlers_.end() ? 0 : it->second.size();
    }

    [[nodiscard]] usize queuedCount() const noexcept { return queued_.size(); }

    void clear() {
        handlers_.clear();
        queued_.clear();
    }

private:
    friend class Subscription;

    struct Handler {
        u64 id;
        std::function<void(const void*)> fn;
    };

    void removeHandler(detail::TypeId type, u64 id) {
        const auto it = handlers_.find(type);
        if (it == handlers_.end()) return;
        std::vector<Handler>& list = it->second;
        for (usize i = 0; i < list.size(); ++i) {
            if (list[i].id == id) {
                list.erase(list.begin() + static_cast<isize>(i));
                return;
            }
        }
    }

    std::unordered_map<detail::TypeId, std::vector<Handler>> handlers_;
    std::vector<std::function<void()>> queued_;
    u64 nextId_ = 1;
};

inline void Subscription::unsubscribe() noexcept {
    if (bus_ != nullptr) {
        bus_->removeHandler(type_, id_);
        bus_ = nullptr;
    }
}

}  // namespace zukiru::event
