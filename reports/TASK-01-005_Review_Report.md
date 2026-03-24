# VSE Platform — TASK-01-005 작업 결과 보고서

> 작성일: 2026-03-24  
> 검토 대상: Sprint 1 — TASK-01-005 IAgentSystem + ECS 컴포넌트  
> 목적: 외부 AI 검증·검토용

---

## 1. 작업 개요

| 항목 | 내용 |
|------|------|
| 태스크 | TASK-01-005: IAgentSystem 인터페이스 + AgentSystem 구현 + ECS 컴포넌트 5종 |
| 레이어 | Layer 0 (IAgentSystem) + Layer 1 Domain (AgentSystem) |
| 완료일 | 2026-03-24 |
| 커밋 | 2a22c9a |
| 테스트 | 15/15 통과 (AgentSystem 전용) |
| 전체 누적 | 66/66 통과 |

---

## 2. 구현 파일

| 파일 | 역할 |
|------|------|
| `include/core/IAgentSystem.h` | 순수 인터페이스 + ECS 컴포넌트 5종 정의 (Layer 0) |
| `include/domain/AgentSystem.h` | AgentSystem 클래스 선언 (Layer 1) |
| `src/domain/AgentSystem.cpp` | AgentSystem 구현 |
| `tests/test_AgentSystem.cpp` | Catch2 테스트 15개 |

---

## 3. ECS 컴포넌트 정의

### 3.1 PositionComponent
```cpp
struct PositionComponent {
    TileCoord tile;
    PixelPos  pixel;                    // 렌더러용 보간 위치 (32px/tile)
    Direction facing = Direction::Right;
};
```
- `tile`: 현재 타일 좌표
- `pixel`: 렌더러가 읽는 보간 위치 — Phase 1에서는 tile × 32
- `facing`: 이동 방향 (렌더링용)

### 3.2 AgentComponent
```cpp
struct AgentComponent {
    AgentState state      = AgentState::Idle;
    TenantType workplace  = TenantType::COUNT;   // COUNT = 미배정
    EntityId   homeTenant = INVALID_ENTITY;
    float      satisfaction = 100.0f;            // 0~100
    float      moveSpeed    = 1.0f;              // tiles/sec
};
```
- `satisfaction`: Phase 1에서는 고정 100 (Phase 2에서 조건부 감소)
- `workplace`: Phase 1에서는 Office 기본값

### 3.3 AgentScheduleComponent
```cpp
struct AgentScheduleComponent {
    int workStartHour = 9;
    int workEndHour   = 18;
    int lunchHour     = 12;
};
```
- SimClock의 `GameTime.hour`와 비교하여 상태 전환 결정

### 3.4 AgentPathComponent
```cpp
struct AgentPathComponent {
    std::vector<TileCoord> path;
    int       currentIndex = 0;
    TileCoord destination  = {0, 0};
};
```
- Phase 1: 경로는 미사용 (이동 없음), Phase 2에서 경로 탐색 구현

### 3.5 ElevatorPassengerComponent
```cpp
struct ElevatorPassengerComponent {
    int  targetFloor = 0;
    bool waiting     = true;   // true=대기, false=탑승 중
};
```
- Phase 1: 엘리베이터 시스템 미구현, 예약 컴포넌트

---

## 4. IAgentSystem 인터페이스

```cpp
class IAgentSystem {
public:
    virtual Result<EntityId> spawnAgent(entt::registry& reg,
                                        EntityId homeTenantId,
                                        EntityId workplaceId,
                                        std::optional<TileCoord> spawnPos = std::nullopt) = 0;
    virtual void despawnAgent(entt::registry& reg, EntityId id) = 0;
    virtual int activeAgentCount() const = 0;
    virtual void update(entt::registry& reg, const GameTime& time) = 0;
    virtual std::optional<AgentState> getState(entt::registry& reg, EntityId id) const = 0;
    virtual std::vector<EntityId> getAgentsOnFloor(entt::registry& reg, int floor) const = 0;
    virtual std::vector<EntityId> getAgentsInState(entt::registry& reg, AgentState state) const = 0;
    virtual float getAverageSatisfaction(entt::registry& reg) const = 0;
};
```

---

## 5. AgentSystem 구현 상세

### 5.1 spawnAgent() — 스폰 위치 결정 로직

```
spawnPos override 있으면 → 해당 좌표 사용
없으면 → GridSystem::getFloorTiles()로 homeTenantId anchor 탐색
  → 찾으면 anchor 좌표에 스폰
  → 못 찾으면 Result::failure(InvalidArgument)
```

- `GridSystem::findAnchor()` 직접 호출 없음 — Layer 경계 준수
- `getFloorTiles()` 순회로 anchor 탐색 (O(floors × floorWidth))
- Phase 1 최대 30층 × 20열 = 600 타일 — 성능 이슈 없음

### 5.2 update() — 스케줄 기반 상태 전환

```
09:00 ~ 12:00  : Idle → Working
12:00          : Working → Resting (점심)
13:00 ~ 18:00  : Resting → Working (오후 근무)
18:00 ~        : Working/Resting → Idle (퇴근)
```

- `GameTime.hour`만 사용 (minute 미사용 — Phase 2에서 세분화 가능)
- 상태 변경 시 `EventBus::publish(AgentStateChanged)` 발행

### 5.3 activeAgents_ 추적

```cpp
std::unordered_set<uint32_t> activeAgents_;
```

- `spawnAgent()` 시 삽입, `despawnAgent()` 시 제거
- `activeAgentCount()`는 O(1)

### 5.4 이벤트 발행

| 시점 | 이벤트 |
|------|--------|
| spawnAgent() | `AgentStateChanged` (초기 Idle) |
| processSchedule() — 상태 변경 시 | `AgentStateChanged` |

---

## 6. 테스트 결과

```
46/60  AgentSystem - 초기 상태 activeAgentCount           PASSED
47/60  AgentSystem - spawnAgent 성공 (homeTenant 위치)     PASSED
48/60  AgentSystem - spawnAgent override 위치              PASSED
49/60  AgentSystem - spawnAgent INVALID_ENTITY → 실패      PASSED
50/60  AgentSystem - spawnAgent 컴포넌트 초기값 확인       PASSED
51/60  AgentSystem - despawnAgent                          PASSED
52/60  AgentSystem - getState                              PASSED
53/60  AgentSystem - getState INVALID_ENTITY → nullopt     PASSED
54/60  AgentSystem - update: 출근 시간 → Working           PASSED
55/60  AgentSystem - update: 점심 시간 → Resting           PASSED
56/60  AgentSystem - update: 퇴근 시간 → Idle              PASSED
57/60  AgentSystem - getAgentsOnFloor                      PASSED
58/60  AgentSystem - getAgentsInState                      PASSED
59/60  AgentSystem - getAverageSatisfaction 에이전트 없음   PASSED
60/60  AgentSystem - 여러 에이전트 동시 관리               PASSED

100% tests passed, 0 tests failed out of 15
```

---

## 7. 설계 원칙 준수 여부

| 원칙 | 준수 여부 | 비고 |
|------|-----------|------|
| exception 사용 금지 | ✅ | 모든 실패 → Result<T> |
| Layer 경계 준수 | ✅ | IAgentSystem(L0), AgentSystem(L1), 게임 규칙 없음 |
| findAnchor() 직접 호출 금지 | ✅ | getFloorTiles() 순회로 대체 |
| fixed-tick update | ✅ | dt 파라미터 없음, GameTime 기반 |
| ECS 컴포넌트 data-only | ✅ | 로직 없음, 순수 데이터 구조체 |
| ConfigManager hot path 호출 금지 | ✅ | AgentSystem에서 ConfigManager 미사용 |
| activeAgentCount() O(1) | ✅ | unordered_set 크기 반환 |

---

## 8. 미결 사항 / 검토 요청 항목

1. **spawnAgent() homeTenant 탐색 비용**: 현재 O(floors × floorWidth) 전체 순회. 에이전트 수 50명 이상이 되면 스폰 빈도가 높을 경우 누적 비용이 될 수 있음. Phase 2에서 GridSystem에 `EntityId → TileCoord` 역방향 인덱스 추가 검토 필요.

2. **update() 시간 해상도**: 현재 `GameTime.hour`만 비교 — minute 단위 정밀도 없음. 출근/퇴근 시각이 정확히 XX:00에만 발동됨. Phase 2에서 `GameTime.minute` 활용 여부 결정 필요.

3. **AgentPathComponent Phase 1 미사용**: 경로 데이터 구조는 정의됐으나 이동 로직 없음. Phase 2에서 경로 탐색 구현 시 인터페이스 확장 여부.

4. **getAverageSatisfaction() registry 의존**: 현재 `entt::registry`를 매 호출마다 순회. StarRatingSystem이 매 틱 호출하면 성능 이슈 가능. 캐싱 전략 검토 필요.

5. **ElevatorPassengerComponent Phase 1 미사용**: 예약 컴포넌트로만 존재. TASK-01-006 TransportSystem 구현 시 연동 방식 확인 필요.

---

## 9. Sprint 1 전체 진행 현황

| Task | 제목 | 상태 |
|------|------|------|
| TASK-01-001 | CMake 프로젝트 구조 | ✅ 완료 |
| TASK-01-002 | SimClock + EventBus | ✅ 완료 |
| TASK-01-003 | ConfigManager + JSON 로딩 | ✅ 완료 |
| TASK-01-004 | IGridSystem + GridSystem | ✅ 완료 (GPT-5.4 수정 반영) |
| TASK-01-005 | IAgentSystem + ECS 컴포넌트 | ✅ 완료 |
| TASK-01-006 | ITransportSystem + ElevatorSystem | ⏳ 대기 |
| TASK-01-007 | IEconomyEngine + EconomyEngine | ⏳ 대기 |
| TASK-01-008 | SDL2 렌더러 초기화 | ⏳ 대기 |
| TASK-01-009 | 타일 렌더링 | ⏳ 대기 |
| TASK-01-010 | 입력 처리 | ⏳ 대기 |

---

*이 보고서는 외부 AI 검증·검토 요청용입니다. 8번 미결 사항 위주로 의견 주시면 됩니다.*

---

## 10. GPT-5.4 Thinking 검토 결과 및 수정 사항 (2026-03-24)

**검토 모델**: GPT-5.4 Thinking  
**판정**: 조건부 승인 → 수정 완료 후 확정 승인

### 핵심 지적 및 조치

| 지적 | 조치 | 커밋 |
|------|------|------|
| `AgentComponent.workplace`가 `TenantType`이어서 같은 타입 테넌트 여러 개일 때 어느 직장인지 특정 불가 | `workplaceTenant EntityId`로 변경 — 실제 목적지 테넌트 엔티티 저장 | 055c561 |
| `spawnAgent()` → `AgentStateChanged` 발행 — 생성 사건과 상태 사건이 혼용 | `AgentSpawned`로 교체 | 055c561 |
| `despawnAgent()` — 이벤트 없음 | `AgentDespawned` 추가 (destroy 전 발행) | 055c561 |

### 확인 사항 (변경 불필요)
- `spawnAgent(reg, homeTenantId, workplaceId, spawnPos)` 시그니처 — VSE_Design_Spec.md §5.5와 완전 일치
- `update(reg, time)` 고정 틱 방식 — 설계 문서 일치
- `getAverageSatisfaction()` Layer 0 공개 — StarRatingSystem 연동 예정, 유지

### 미결 사항 처리 방향

| # | 항목 | 결론 |
|---|------|------|
| 1 | spawnAgent homeTenant 탐색 비용 | Phase 1 유지 (600타일, NPC 50명 이하) |
| 2 | update() minute 정밀도 | TransportSystem 연동 전 세분화 |
| 3 | AgentPathComponent 미사용 | Phase 2에서 경로 탐색 구현 시 확장 |
| 4 | getAverageSatisfaction() 순회 | 캐싱 불필요 (NPC 50명 규모) |
| 5 | ElevatorPassengerComponent 미사용 | TASK-01-006 연동 시 재검토 |

### 최종 테스트 현황
- AgentSystem: 15/15 (workplaceTenant 저장 확인 테스트 포함)
- 전체 누적: **66/66**
