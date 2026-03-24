#pragma once
#include "core/ITransportSystem.h"
#include "core/IGridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <entt/entt.hpp>
#include <unordered_map>
#include <vector>
#include <set>

namespace vse {

/**
 * TransportSystem — Phase 1 엘리베이터 구현체.
 *
 * 내부 구조:
 * - ElevatorCar: 엘리베이터 1대 상태 (FSM + 탑승객 + 콜 큐)
 * - LookScheduler: LOOK 알고리즘 다음 정지 층 계산
 *
 * EntityId 발급 정책:
 * - 엘리베이터 EntityId는 내부 entt::registry에서 발급
 * - AgentSystem 등 다른 registry와 충돌 없음 (독립 registry)
 * - 외부에서는 EntityId 값만 사용, 내부 registry는 노출 금지
 *
 * callElevator() 다중 car 배차 정책 (Phase 1):
 * - 같은 shaftX에 여러 car가 있으면 최적 car 선택
 * - 기준: Idle 우선, 이동 중이면 현재 방향과 일치하는 car, 거리 최단
 *
 * Phase 1 제약:
 * - doorOpenTicks: config "elevator.doorOpenTicks" (기본 3틱)
 * - speedTilesPerTick: config "elevator.speedTilesPerTick" (기본 1)
 * - floatFloor: 현재 Phase 1에서는 currentFloor와 동일 (미구현 placeholder)
 *   Phase 3 렌더러 연동 시 틱 간 보간으로 교체 예정
 */
class TransportSystem : public ITransportSystem {
public:
    TransportSystem(IGridSystem& grid, EventBus& bus, const ConfigManager& config);

    Result<EntityId> createElevator(int shaftX, int bottomFloor,
                                     int topFloor, int capacity) override;
    void callElevator(int shaftX, int floor,
                       ElevatorDirection preferredDir) override;
    Result<bool> boardPassenger(EntityId elevator,
                                 EntityId agent, int targetFloor) override;
    void exitPassenger(EntityId elevator, EntityId agent) override;
    void update(const GameTime& time) override;

    std::optional<ElevatorSnapshot> getElevatorState(EntityId id) const override;
    std::vector<EntityId> getElevatorsAtFloor(int floor) const override;
    int getWaitingCount(int floor) const override;
    std::vector<EntityId> getAllElevators() const override;

private:
    // ── ElevatorCar — 엘리베이터 1대 내부 상태 ────────────────────────────
    struct HallCall {
        int               floor;
        ElevatorDirection direction;
        bool operator<(const HallCall& o) const {
            if (floor != o.floor) return floor < o.floor;
            return static_cast<int>(direction) < static_cast<int>(o.direction);
        }
    };

    struct ElevatorCar {
        EntityId          id;
        int               shaftX;
        int               bottomFloor;
        int               topFloor;
        int               capacity;
        int               currentFloor   = 0;
        float             floatFloor     = 0.0f;  // Phase 1: currentFloor와 동일 (렌더러 보간 placeholder)
        ElevatorState     state          = ElevatorState::Idle;
        ElevatorDirection direction      = ElevatorDirection::Idle;
        int               doorTicks      = 0;     // 남은 door open 틱
        int               nextTarget     = -1;    // LOOK 다음 정지 층
        std::vector<EntityId> passengers;
        std::set<int>     carCalls;       // 탑승객이 요청한 목적지 층
        std::set<HallCall> hallCalls;     // 홀 콜 큐
    };

    IGridSystem&    grid_;
    EventBus&       eventBus_;
    int             doorOpenTicks_;
    int             speedTilesPerTick_;
    int             defaultCapacity_;

    // EntityId 발급용 독립 registry (AgentSystem registry와 독립)
    entt::registry  registry_;
    // EntityId (uint32) → ElevatorCar
    std::unordered_map<uint32_t, ElevatorCar> cars_;
    // floor → 대기 인원 수
    std::unordered_map<int, int> waitingCount_;

    // ── Internal helpers ───────────────────────────────────────────────────
    void tickCar(ElevatorCar& car, const GameTime& time);
    int  lookNextTarget(const ElevatorCar& car) const;
    void publishEvent(EventType type, EntityId source);
    // 같은 shaftX의 car 목록에서 콜에 응답할 최적 car 선택
    // 반환: cars_ key (uint32_t), 없으면 UINT32_MAX
    uint32_t pickBestCar(int shaftX, int floor, ElevatorDirection dir) const;
};

} // namespace vse
