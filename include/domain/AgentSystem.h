#pragma once
#include "core/IAgentSystem.h"
#include "core/IGridSystem.h"
#include "core/ITransportSystem.h"
#include "core/EventBus.h"
#include <unordered_set>

namespace vse {

/**
 * AgentSystem — Phase 1 NPC 에이전트 구현체.
 *
 * Phase 1 행동 모델:
 * - 에이전트는 같은 층 내 수평 이동만 수행 (엘리베이터 없음)
 * - 스케줄 기반 상태 전환: Idle ↔ Working ↔ AtLunch
 * - update()는 GameTime.hour 기준으로 스케줄 체크
 * - satisfaction은 현재 고정 100 (Phase 2에서 조건부 감소)
 *
 * 의존성:
 * - IGridSystem: 목적지 위치 조회, 경로 검증
 * - EventBus: 상태 변경 이벤트 발행
 * - ITransportSystem (optional): 엘리베이터 다층 이동 지원
 *   nullptr이면 같은 층 이동만 수행 (하위 호환성 유지)
 */
class AgentSystem : public IAgentSystem {
public:
    // 기존 생성자 — transport 없음 (같은 층 이동 전용, 기존 테스트 호환)
    AgentSystem(IGridSystem& grid, EventBus& bus);

    // 엘리베이터 지원 생성자 — transport 있음
    AgentSystem(IGridSystem& grid, EventBus& bus, ITransportSystem& transport);

    Result<EntityId> spawnAgent(entt::registry& reg,
                                EntityId homeTenantId,
                                EntityId workplaceId,
                                std::optional<TileCoord> spawnPos = std::nullopt) override;

    void despawnAgent(entt::registry& reg, EntityId id) override;

    int activeAgentCount() const override;

    void update(entt::registry& reg, const GameTime& time) override;

    std::optional<AgentState> getState(entt::registry& reg, EntityId id) const override;

    std::vector<EntityId> getAgentsOnFloor(entt::registry& reg, int floor) const override;

    std::vector<EntityId> getAgentsInState(entt::registry& reg, AgentState state) const override;

    float getAverageSatisfaction(const entt::registry& reg) const override;

    // ── SaveLoad support ───────────────────────────────────────────────────
    /** Register a restored agent in the activeAgents_ tracking set. */
    void registerRestoredAgent(EntityId id);
    /** Clear activeAgents_ tracking (called before load). */
    void clearTracking();

private:
    IGridSystem&      grid_;
    EventBus&         eventBus_;
    ITransportSystem* transport_;   // optional — nullptr = 같은 층 이동 전용

    // 추적 중인 에이전트 ID 집합 (activeAgentCount용)
    std::unordered_set<uint32_t> activeAgents_;

    // 스케줄 체크 — 시간 기반 상태 전환
    void processSchedule(entt::registry& reg, EntityId id,
                         AgentComponent& agent,
                         const AgentScheduleComponent& schedule,
                         const GameTime& time);

    // 엘리베이터 대기/탑승/하차 처리
    void processElevator(entt::registry& reg, EntityId id,
                         AgentComponent& agent,
                         const AgentScheduleComponent& schedule,
                         const GameTime& time);

    // 만족도/스트레스 갱신 및 이탈 체크
    void updateSatisfactionAndStress(entt::registry& reg, EntityId id,
                                     AgentComponent& agent,
                                     const AgentScheduleComponent& schedule);

    // 목적지 타일 조회 (anchor 위치)
    std::optional<TileCoord> resolveDestination(EntityId tenantEntityId) const;

    // 테넌트 점유자 수 업데이트 (Working 상태 변경 시)
    void updateTenantOccupantCount(entt::registry& reg, EntityId agentId,
                                   AgentState oldState, AgentState newState);
};

} // namespace vse
