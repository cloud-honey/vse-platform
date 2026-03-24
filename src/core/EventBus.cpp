#include "core/EventBus.h"
#include <algorithm>
#include <cassert>

namespace vse {

SubscriptionId EventBus::subscribe(EventType type, EventCallback callback) {
    Subscription sub;
    sub.id = nextId_++;
    sub.callback = std::move(callback);
    
    subscribers_[type].push_back(sub);
    return sub.id;
}

void EventBus::unsubscribe(SubscriptionId id) {
    for (auto& pair : subscribers_) {
        auto& subs = pair.second;
        auto it = std::find_if(subs.begin(), subs.end(),
            [id](const Subscription& sub) { return sub.id == id; });
        
        if (it != subs.end()) {
            subs.erase(it);
            return;
        }
    }
}

void EventBus::publish(Event event) {
    queue_.push_back(std::move(event));
}

void EventBus::flush() {
    // queue_와 processing_ 스왑
    std::swap(queue_, processing_);
    
    // processing_의 모든 이벤트 처리
    for (const auto& event : processing_) {
        auto it = subscribers_.find(event.type);
        if (it != subscribers_.end()) {
            // 구독자 목록을 복사하여 핸들러 내부에서 unsubscribe 가능하도록 함
            auto subs = it->second;
            for (const auto& sub : subs) {
                sub.callback(event);
            }
        }
    }
    
    processing_.clear();
}

size_t EventBus::pendingCount() const {
    return queue_.size();
}

size_t EventBus::subscriberCount(EventType type) const {
    auto it = subscribers_.find(type);
    if (it != subscribers_.end()) {
        return it->second.size();
    }
    return 0;
}

} // namespace vse