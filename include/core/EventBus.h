#pragma once
#include "core/Types.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>
#include <cstdint>

namespace vse {

/**
 * EventBus — deferred 이벤트 큐. 시스템 간 결합도를 낮추는 메시지 버스.
 *
 * 전달 방식 (Phase 1 — deferred-only):
 *   - publish()는 항상 queue_에 추가만 함 (즉시 전달 금지)
 *   - flush()에서 queue_ ↔ processing_ 스왑 후 전달
 *   - tick N에서 발행된 이벤트는 tick N+1 시작 시 flush()로 소비됨
 *
 * double-buffer 구조:
 *   - flush() 중 새로 publish()된 이벤트는 현재 flush에 영향 없음
 *   - 다음 flush에서 처리됨 → 무한루프 방지
 *
 * 구독 관리:
 *   - subscribe() → SubscriptionId 반환
 *   - unsubscribe(id) → 구독 해제 (flush 중에도 안전)
 *
 * Phase 3 예정:
 *   - replicable 이벤트 네트워크 동기화 (현재 미구현)
 */

struct Event {
    EventType type;
    bool      replicable = false;        // Phase 3: 네트워크 동기화 대상 여부
    EntityId  source     = INVALID_ENTITY;
    std::any  payload;                   // 타입 소거 페이로드 (선택적)
};

using EventCallback  = std::function<void(const Event&)>;
using SubscriptionId = uint32_t;

class EventBus {
public:
    // 이벤트 타입에 콜백 등록. 반환된 id로 해제 가능.
    SubscriptionId subscribe(EventType type, EventCallback callback);

    // 구독 해제. flush 중 호출해도 안전 (구독자 목록 복사 후 처리).
    void unsubscribe(SubscriptionId id);

    // 이벤트 발행 — 항상 deferred (queue_에 추가만 함, 즉시 전달 금지)
    void publish(Event event);

    // 큐 전달 — tick 시작 시 1회 호출.
    // queue_ ↔ processing_ 스왑 후 processing_ 순회하여 콜백 호출.
    void flush();

    // 디버그/통계용
    size_t pendingCount()                    const;  // 현재 queue_ 대기 이벤트 수
    size_t subscriberCount(EventType type)   const;  // 특정 타입 구독자 수

private:
    struct Subscription {
        SubscriptionId id;
        EventCallback  callback;
    };

    std::unordered_map<EventType, std::vector<Subscription>> subscribers_;
    std::vector<Event> queue_;       // publish() 대상 — 다음 flush 대기
    std::vector<Event> processing_;  // flush() 처리 중 버퍼 (double-buffer)
    SubscriptionId nextId_ = 1;
};

} // namespace vse
