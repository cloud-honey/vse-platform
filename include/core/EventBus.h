#pragma once
#include "core/Types.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>
#include <cstdint>

namespace vse {

struct Event {
    EventType type;
    bool replicable = false;
    EntityId source = INVALID_ENTITY;
    std::any payload;
};

using EventCallback = std::function<void(const Event&)>;
using SubscriptionId = uint32_t;

class EventBus {
public:
    SubscriptionId subscribe(EventType type, EventCallback callback);
    void unsubscribe(SubscriptionId id);
    void publish(Event event);   // 항상 deferred
    void flush();                // tick 시작 시 호출 — queue_ → processing_ 스왑 후 전달

    size_t pendingCount() const;
    size_t subscriberCount(EventType type) const;

private:
    struct Subscription {
        SubscriptionId id;
        EventCallback callback;
    };

    std::unordered_map<EventType, std::vector<Subscription>> subscribers_;
    std::vector<Event> queue_;
    std::vector<Event> processing_;
    SubscriptionId nextId_ = 1;
};

} // namespace vse