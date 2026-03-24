# VSE Platform — TASK-01-006 작업 결과 보고서

> 작성일: 2026-03-24  
> 검토 대상: Sprint 1 — TASK-01-006 ITransportSystem + ElevatorSystem  
> 목적: 외부 AI 검증·검토용

---

## 1. 작업 개요

| 항목 | 내용 |
|------|------|
| 태스크 | TASK-01-006: ITransportSystem 인터페이스 + TransportSystem 구현 |
| 레이어 | Layer 0 (ITransportSystem) + Layer 1 Domain (TransportSystem) |
| 완료일 | 2026-03-24 |
| 커밋 | 66dda36 |
| 테스트 | 15/15 통과 (TransportSystem 전용) |
| 전체 누적 | 81/81 통과 |

---

## 2. 구현 파일

| 파일 | 역할 |
|------|------|
| `include/core/Types.h` | `ElevatorState` enum 추가 |
| `assets/config/game_config.json` | `elevator.doorOpenTicks` 추가 |
| `include/core/ITransportSystem.h` | 순수 인터페이스 + ElevatorSnapshot (Layer 0) |
| `include/domain/TransportSystem.h` | TransportSystem 클래스 선언 (Layer 1) |
| `src/domain/TransportSystem.cpp` | TransportSystem 구현 |
| `tests/test_TransportSystem.cpp` | Catch2 테스트 15개 |

---

## 3. ElevatorState enum (Types.h 추가)

```cpp
// Elevator FSM 상태 (authoritative)
// Idle → MovingUp/MovingDown → DoorOpening → Boarding → DoorClosing → (Idle or Moving)
enum class ElevatorState : uint8_t {
    Idle = 0,
    MovingUp,
    MovingDown,
    DoorOpening,
    Boarding,
    DoorClosing
};
```

---

## 4. ITransportSystem 인터페이스

```cpp
class ITransportSystem {
public:
    // Setup
    virtual Result<EntityId> createElevator(int shaftX, int bottomFloor,
                                             int topFloor, int capacity) = 0;
    // Operations
    virtual void callElevator(int shaftX, int floor, ElevatorDirection preferredDir) = 0;
    virtual Result<bool> boardPassenger(EntityId elevator, EntityId agent, int targetFloor) = 0;
    virtual void exitPassenger(EntityId elevator, EntityId agent) = 0;
    virtual void update(const GameTime& time) = 0;
    // Query
    virtual std::optional<ElevatorSnapshot> getElevatorState(EntityId id) const = 0;
    virtual std::vector<EntityId> getElevatorsAtFloor(int floor) const = 0;
    virtual int getWaitingCount(int floor) const = 0;
    virtual std::vector<EntityId> getAllElevators() const = 0;
};
```

---

## 5. TransportSystem 구현 상세

### 5.1 내부 자료구조

```cpp
struct ElevatorCar {
    EntityId          id;
    int               shaftX;
    int               bottomFloor / topFloor / capacity;
    int               currentFloor;
    float             floatFloor;       // 렌더러용 보간 위치
    ElevatorState     state;
    ElevatorDirection direction;
    int               doorTicks;        // Boarding 남은 틱
    int               nextTarget;       // LOOK 다음 정지 층
    std::vector<EntityId> passengers;
    std::set<int>     carCalls;         // 탑승객 목적지 층
    std::set<HallCall> hallCalls;       // 홀 콜 큐 (중복 자동 병합)
};
```

- entt::registry 미사용 — 단순 uint32 카운터로 EntityId 발급
- `std::set<HallCall>`: 같은 (shaftX, floor, direction) 중복 호출 자동 병합

### 5.2 Elevator FSM (ElevatorState 전이)

```
Idle
 └── callQueue 있으면 → nextTarget 결정
      ├── nextTarget > currentFloor → MovingUp
      ├── nextTarget < currentFloor → MovingDown
      └── nextTarget == currentFloor → DoorOpening

MovingUp / MovingDown
 └── speedTilesPerTick 이동
      └── 도착 → DoorOpening + ElevatorArrived 이벤트

DoorOpening
 └── 1틱 → Boarding (문 완전 열림)

Boarding
 └── doorTicks 카운트다운
      └── 0 → 현재 층 홀콜/카콜 제거 → DoorClosing

DoorClosing
 ├── queue 없음 → Idle
 ├── 같은 층 콜 → DoorOpening
 └── 다른 층 콜 → MovingUp/MovingDown
```

### 5.3 LOOK 알고리즘 (lookNextTarget)

```
모든 콜(carCalls + hallCalls) 수집
 ├── 현재 방향과 같은 쪽 목표 있으면 → 가장 가까운 것 먼저
 └── 없으면 → 반대 방향 중 현재 위치에서 가장 가까운 것
```

- Up 방향: `currentFloor`보다 큰 것 중 최솟값
- Down 방향: `currentFloor`보다 작은 것 중 최댓값
- Idle 상태: 방향 무관 가장 가까운 것

### 5.4 ConfigManager 연동

| Config 키 | 기본값 | 의미 |
|-----------|--------|------|
| `elevator.doorOpenTicks` | 3 | Boarding 상태 유지 틱 수 |
| `elevator.speedTilesPerTick` | 1 | 틱당 이동 층 수 |
| `elevator.capacity` | 8 | 최대 탑승 인원 |

### 5.5 이벤트 발행

| 시점 | 이벤트 |
|------|--------|
| createElevator() | `ElevatorArrived` (초기 생성 알림) |
| 목적 층 도착 (MovingUp/Down → DoorOpening) | `ElevatorArrived` |

---

## 6. 테스트 결과

```
67/81  TransportSystem - 초기 상태 엘리베이터 없음           PASSED
68/81  TransportSystem - createElevator 성공                  PASSED
69/81  TransportSystem - createElevator 중복 → 실패           PASSED
70/81  TransportSystem - createElevator 잘못된 범위 → 실패    PASSED
71/81  TransportSystem - 생성 직후 Idle 상태                  PASSED
72/81  TransportSystem - callElevator → 이동 시작             PASSED
73/81  TransportSystem - 목적 층 도착 → DoorOpening           PASSED
74/81  TransportSystem - DoorOpening → Boarding               PASSED
75/81  TransportSystem - boardPassenger 성공                   PASSED
76/81  TransportSystem - boardPassenger Boarding 아닐 때 실패  PASSED
77/81  TransportSystem - boardPassenger 정원 초과 → 실패       PASSED
78/81  TransportSystem - exitPassenger                         PASSED
79/81  TransportSystem - getWaitingCount                       PASSED
80/81  TransportSystem - getElevatorsAtFloor                   PASSED
81/81  TransportSystem - LOOK: 위로 이동 중 위쪽 콜 먼저      PASSED

100% tests passed, 0 tests failed out of 15
```

---

## 7. 설계 원칙 준수 여부

| 원칙 | 준수 여부 | 비고 |
|------|-----------|------|
| exception 사용 금지 | ✅ | 모든 실패 → Result<T> |
| Layer 경계 준수 | ✅ | ITransportSystem(L0), TransportSystem(L1) |
| fixed-tick update | ✅ | dt 없음, GameTime 파라미터 |
| ElevatorCarFSM 내부 분리 | ✅ | tickCar() + lookNextTarget() 내부 private |
| LookScheduler 내부 분리 | ✅ | lookNextTarget() 내부 private |
| 홀 콜 중복 병합 | ✅ | std::set<HallCall> 자동 병합 |
| 정원 초과 처리 | ✅ | boardPassenger() failure 반환 |
| ConfigManager 캐싱 | ✅ | 생성자에서 로컬 변수 저장 |

---

## 8. 미결 사항 / 검토 요청 항목

1. **엘리베이터 ID 발급 방식**: 현재 단순 uint32 카운터로 EntityId 발급 (entt::registry 미사용). AgentSystem 등 다른 시스템과 EntityId 충돌 가능성 검토 필요.

2. **중간 층 LOOK 정차 처리**: 현재 MovingUp/Down 중 중간 층 홀 콜 처리 시 `hallCalls` set 순회로 처리. 동시에 여러 층이 같은 방향으로 호출됐을 때 모두 처리되는지 검증 추가 필요.

3. **엘리베이터 이동 보간 (floatFloor)**: 현재 `floatFloor`는 `currentFloor`와 동일값으로 설정 (정수). Phase 2 렌더링에서 틱 간 보간 필요 시 별도 처리 로직 추가 필요.

4. **AgentSystem ↔ TransportSystem 연동**: 현재 에이전트가 엘리베이터를 직접 호출하거나 탑승하는 로직 없음. Phase 2에서 AgentSystem이 목적지 층이 다를 때 callElevator() → boardPassenger() 호출 시퀀스 구현 필요.

5. **여러 shaft 처리**: 현재 `callElevator(shaftX, ...)` 호출 시 해당 shaftX의 첫 번째 엘리베이터만 콜 추가. 같은 shaftX에 여러 엘리베이터 운용 시나리오 미검증.

6. **ElevatorPassengerComponent 연동**: 에이전트의 `ElevatorPassengerComponent.waiting` 필드 업데이트 로직 미구현 (TransportSystem이 AgentSystem registry에 직접 접근 불가).

---

## 9. Sprint 1 전체 진행 현황

| Task | 제목 | 상태 |
|------|------|------|
| TASK-01-001 | CMake 프로젝트 구조 | ✅ 완료 |
| TASK-01-002 | SimClock + EventBus | ✅ 완료 |
| TASK-01-003 | ConfigManager + JSON 로딩 | ✅ 완료 |
| TASK-01-004 | IGridSystem + GridSystem | ✅ 완료 |
| TASK-01-005 | IAgentSystem + ECS 컴포넌트 | ✅ 완료 |
| TASK-01-006 | ITransportSystem + ElevatorSystem | ✅ 완료 |
| TASK-01-007 | IEconomyEngine + EconomyEngine | ⏳ 대기 |
| TASK-01-008 | SDL2 렌더러 초기화 | ⏳ 대기 |
| TASK-01-009 | 타일 렌더링 | ⏳ 대기 |
| TASK-01-010 | 입력 처리 | ⏳ 대기 |

---

*이 보고서는 외부 AI 검증·검토 요청용입니다. 8번 미결 사항 위주로 의견 주시면 됩니다.*
