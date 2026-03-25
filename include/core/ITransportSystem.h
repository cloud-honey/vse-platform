#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <vector>
#include <optional>

namespace vse {

/**
 * ElevatorSnapshot — 렌더러 및 외부 시스템용 엘리베이터 상태 스냅샷.
 * 내부 FSM 상태를 외부에 노출하는 값 타입.
 */
struct ElevatorSnapshot {
    EntityId              id;
    int                   shaftX;           // 렌더러용 shaft 위치
    int                   currentFloor;
    float                 interpolatedFloor;  // 렌더러용 보간 위치
    ElevatorState         state;
    ElevatorDirection     direction;
    int                   passengerCount;
    int                   capacity;
    std::vector<EntityId> passengers;
    std::vector<int>      callQueue;          // 남은 목적지 층 목록
};

/**
 * ITransportSystem — 엘리베이터 생성·운행·탑승 관리 인터페이스.
 *
 * 설계 원칙:
 * - createElevator(): GridSystem에 이미 placeElevatorShaft()된 shaftX에 엘리베이터 생성
 * - callElevator(): 홀 콜 (floor에서 특정 방향으로 호출)
 * - boardPassenger(): 에이전트가 엘리베이터에 탑승 (targetFloor 등록)
 * - update(): LOOK 알고리즘 + FSM 틱 처리. dt 없음 (fixed-tick).
 * - exception 사용 금지 — 모든 실패 Result<T> 반환
 *
 * 내부 구현 분리 (ITransportSystem 외부에 노출되지 않음):
 * - ElevatorCarFSM: 엘리베이터 1대의 상태 머신
 * - LookScheduler: 홀 콜 + 카 콜 수집, LOOK 다음 정지 층 결정
 *
 * Elevator State Machine:
 *   Idle → (call received) → MovingUp/MovingDown
 *   MovingUp/MovingDown → (arrived) → DoorOpening
 *   DoorOpening → (door open) → Boarding
 *   Boarding → (doorOpenTicks elapsed) → DoorClosing
 *   DoorClosing → (queue empty) → Idle
 *   DoorClosing → (queue not empty) → MovingUp/MovingDown
 */
class ITransportSystem {
public:
    virtual ~ITransportSystem() = default;

    // ── Setup ──────────────────────────────────────────────────────────────
    /**
     * 엘리베이터 생성.
     * @param shaftX       GridSystem에 설치된 shaft x 좌표
     * @param bottomFloor  운행 최저 층
     * @param topFloor     운행 최고 층
     * @param capacity     최대 탑승 인원
     * @return 생성된 엘리베이터 EntityId
     */
    virtual Result<EntityId> createElevator(int shaftX, int bottomFloor,
                                             int topFloor, int capacity) = 0;

    // ── Operations ─────────────────────────────────────────────────────────
    /**
     * 홀 콜 — 특정 층에서 엘리베이터 호출.
     * 같은 shaftX + floor + direction 중복 호출은 병합.
     */
    virtual void callElevator(int shaftX, int floor,
                               ElevatorDirection preferredDir) = 0;

    /**
     * 에이전트 탑승 — Boarding 상태일 때만 성공.
     * 정원 초과 시 failure 반환. 에이전트는 WaitingElevator 상태 유지.
     */
    virtual Result<bool> boardPassenger(EntityId elevator,
                                         EntityId agent, int targetFloor) = 0;

    /**
     * 에이전트 하차 — 목적지 층 도착 시 호출.
     */
    virtual void exitPassenger(EntityId elevator, EntityId agent) = 0;

    /**
     * 틱 갱신 — LOOK 알고리즘 + FSM 진행.
     * fixed-tick 방식, dt 없음.
     */
    virtual void update(const GameTime& time) = 0;

    // ── Query ──────────────────────────────────────────────────────────────
    /** 엘리베이터 현재 상태 스냅샷 */
    virtual std::optional<ElevatorSnapshot> getElevatorState(EntityId id) const = 0;

    /** 특정 층에 있는(또는 도착 예정인) 엘리베이터 목록 */
    virtual std::vector<EntityId> getElevatorsAtFloor(int floor) const = 0;

    /** 특정 층 대기 인원 수 */
    virtual int getWaitingCount(int floor) const = 0;

    /** 전체 엘리베이터 목록.
     *  Phase 1 API decision: per-ID query via getElevatorState() is sufficient.
     *  Bulk snapshot API deferred to Phase 2 if performance requires it. */
    virtual std::vector<EntityId> getAllElevators() const = 0;
};

} // namespace vse
