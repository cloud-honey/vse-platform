#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <entt/entt.hpp>
#include <vector>
#include <optional>

namespace vse {

// ── ECS Components (data only, no logic) ────────────────────────────────────

/**
 * PositionComponent — 에이전트의 타일 위치 및 픽셀 보간 위치.
 * 렌더러가 픽셀 위치를 읽어 부드러운 이동 표현.
 * AgentSystem이 update() 시 tile → pixel 보간 갱신.
 */
struct PositionComponent {
    TileCoord tile;
    PixelPos  pixel;                         // 렌더러용 보간 위치
    Direction facing = Direction::Right;
};

/**
 * AgentComponent — 에이전트의 상태, 직장/거주지, 만족도.
 * satisfaction: 0~100, 100이 최대 만족.
 * moveSpeed: 초당 타일 이동 속도 (기본 1.0).
 */
struct AgentComponent {
    AgentState state      = AgentState::Idle;
    TenantType workplace  = TenantType::COUNT;   // COUNT = 미배정
    EntityId   homeTenant = INVALID_ENTITY;
    float      satisfaction = 100.0f;
    float      moveSpeed    = 1.0f;              // tiles/sec
};

/**
 * AgentScheduleComponent — 일과 시간표.
 * SimClock의 GameTime.hour와 비교하여 행동 결정.
 */
struct AgentScheduleComponent {
    int workStartHour = 9;
    int workEndHour   = 18;
    int lunchHour     = 12;
};

/**
 * AgentPathComponent — 경로 탐색 결과 저장.
 * path: TileCoord 시퀀스 (시작 포함, 도착 포함).
 * currentIndex: 현재 이동 중인 path 인덱스.
 * destination: 최종 목적지.
 */
struct AgentPathComponent {
    std::vector<TileCoord> path;
    int       currentIndex = 0;
    TileCoord destination  = {0, 0};
};

/**
 * ElevatorPassengerComponent — 엘리베이터 탑승/대기 상태.
 * waiting=true: 샤프트 앞 대기 중
 * waiting=false: 엘리베이터 탑승 중
 */
struct ElevatorPassengerComponent {
    int  targetFloor = 0;
    bool waiting     = true;
};

// ── IAgentSystem Interface ───────────────────────────────────────────────────

/**
 * IAgentSystem — NPC 에이전트 생성·갱신·조회 인터페이스.
 *
 * 설계 원칙:
 * - spawnAgent()는 entt::registry에 엔티티 생성 + 컴포넌트 부착
 * - update()는 고정 틱 기반 (fixed-tick = TICK_MS), dt 파라미터 없음
 * - GameTime을 SimClock에서 받아 일과 스케줄 결정
 * - exception 사용 금지 — 모든 실패 Result<T> 반환
 *
 * Phase 1 에이전트 행동:
 * - Idle → Working (workStartHour 도달)
 * - Working → Idle (workEndHour 도달)
 * - 경로 탐색은 단순 수평 이동 (엘리베이터 없이 같은 층만)
 */
class IAgentSystem {
public:
    virtual ~IAgentSystem() = default;

    /**
     * 에이전트 스폰.
     * @param reg      ECS 레지스트리
     * @param homeTenantId   거주지 테넌트 엔티티 ID
     * @param workplaceId    직장 테넌트 엔티티 ID
     * @param spawnPos       스폰 위치 override (없으면 homeTenant 위치 사용)
     * @return 생성된 에이전트의 EntityId
     */
    virtual Result<EntityId> spawnAgent(entt::registry& reg,
                                        EntityId homeTenantId,
                                        EntityId workplaceId,
                                        std::optional<TileCoord> spawnPos = std::nullopt) = 0;

    /**
     * 에이전트 제거. registry에서 엔티티 destroy.
     */
    virtual void despawnAgent(entt::registry& reg, EntityId id) = 0;

    /** 현재 스폰된 에이전트 수 */
    virtual int activeAgentCount() const = 0;

    /**
     * 틱 갱신. SimClock의 GameTime 기반으로 스케줄·이동 처리.
     * fixed-tick 방식 — dt 없음.
     */
    virtual void update(entt::registry& reg, const GameTime& time) = 0;

    /** 에이전트 현재 상태 조회 */
    virtual std::optional<AgentState> getState(entt::registry& reg, EntityId id) const = 0;

    /** 특정 층의 에이전트 목록 */
    virtual std::vector<EntityId> getAgentsOnFloor(entt::registry& reg, int floor) const = 0;

    /** 특정 상태의 에이전트 목록 */
    virtual std::vector<EntityId> getAgentsInState(entt::registry& reg, AgentState state) const = 0;

    /** 전체 에이전트 평균 만족도 (StarRatingSystem용) */
    virtual float getAverageSatisfaction(entt::registry& reg) const = 0;
};

} // namespace vse
