# VSE Platform — TASK-01-004 작업 결과 보고서

> 작성일: 2026-03-24  
> 검토 대상: Sprint 1 — TASK-01-004 IGridSystem + GridSystem  
> 목적: 외부 AI 검증·검토용

---

## 1. 작업 개요

| 항목 | 내용 |
|------|------|
| 태스크 | TASK-01-004: IGridSystem 인터페이스 + GridSystem 구현 |
| 레이어 | Layer 0 (IGridSystem) + Layer 1 Domain (GridSystem) |
| 완료일 | 2026-03-24 |
| 커밋 | ad8ab8d |
| 테스트 | 18/18 통과 (GridSystem 전용) |
| 전체 누적 | 45/45 통과 (SimClock 14 + ConfigManager 13 + GridSystem 18) |

---

## 2. 트러블슈팅 기록 (중요)

### 문제: 붐2(DeepSeek) 서브에이전트가 EventBus, ConfigManager를 pure abstract interface로 변경
- **원인**: 붐2가 테스트에서 Mock 상속을 쉽게 하려고 기존 concrete 클래스 2개를 pure virtual interface로 변경
- **영향**: test_ConfigManager.cpp의 기존 27개 테스트 전체 빌드 실패
- **복구**: `include/core/EventBus.h`, `include/core/ConfigManager.h` 원래 concrete 클래스로 복원
- **교훈**: 붐2 작업 후 반드시 기존 헤더 파일 손상 여부 점검 필요

### 문제: 테스트 파일 불완전 (파일 잘림)
- 붐2가 생성한 test_GridSystem.cpp가 중간에 잘려서 컴파일 오류
- 붐 직접 재작성으로 해결

---

## 3. 구현 파일

| 파일 | 역할 |
|------|------|
| `include/core/IGridSystem.h` | 순수 인터페이스 (Layer 0) |
| `include/domain/GridSystem.h` | GridSystem 클래스 선언 (Layer 1) |
| `src/domain/GridSystem.cpp` | GridSystem 구현 |
| `tests/test_GridSystem.cpp` | Catch2 테스트 18개 |

---

## 4. IGridSystem 인터페이스

```cpp
struct TileData {
    TenantType tenantType = TenantType::COUNT;  // COUNT = empty
    EntityId tenantEntity = INVALID_ENTITY;
    bool isAnchor = false;
    bool isLobby = false;
    bool isElevatorShaft = false;
    int tileWidth = 1;  // anchor 타일에서만 유효
};

class IGridSystem {
public:
    virtual ~IGridSystem() = default;

    virtual int maxFloors() const = 0;
    virtual int floorWidth() const = 0;

    virtual Result<bool> buildFloor(int floor) = 0;
    virtual bool isFloorBuilt(int floor) const = 0;
    virtual int builtFloorCount() const = 0;

    virtual Result<bool> placeTenant(TileCoord anchor, TenantType type, int width, EntityId entity) = 0;
    virtual Result<bool> removeTenant(TileCoord anyTile) = 0;
    virtual std::optional<TileData> getTile(TileCoord pos) const = 0;
    virtual bool isTileEmpty(TileCoord pos) const = 0;
    virtual bool isValidCoord(TileCoord pos) const = 0;
    virtual std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const = 0;

    virtual Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) = 0;
    virtual bool isElevatorShaft(TileCoord pos) const = 0;

    virtual std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const = 0;
    virtual std::optional<TileCoord> findAnchor(TileCoord anyTile) const = 0;
};
```

---

## 5. GridSystem 구현 상세

### 5.1 내부 자료구조

```cpp
struct FloorData {
    std::vector<TileData> tiles;  // floorWidth_ 크기 고정
    bool built = false;
};
std::unordered_map<int, FloorData> floors_;  // key = floor 번호
```

- 층 번호를 key로 하는 해시맵 — 미건설 층은 맵에 없음
- 각 층은 `floorWidth_` 크기의 `TileData` 벡터
- 층 0은 생성자에서 자동 초기화 (로비)

### 5.2 ConfigManager 연동

```cpp
GridSystem::GridSystem(EventBus& bus, const ConfigManager& config)
    : eventBus_(bus)
    , maxFloors_(config.getInt("simulation.maxFloors", 10))
    , floorWidth_(config.getInt("grid.tilesPerFloor", 20))
```

- 생성자에서 값 추출 후 로컬 변수에 캐싱 → hot path에서 ConfigManager 반복 호출 없음
- game_config.json 기준: maxFloors=30, tilesPerFloor=20

### 5.3 멀티타일 테넌트 occupancy 규칙

```cpp
// placeTenant() — width=3, anchor=(5,1)
for (int i = 0; i < width; ++i) {
    TileData tile;
    tile.tenantType  = type;
    tile.tenantEntity = entity;
    tile.isAnchor    = (i == 0);   // 왼쪽 타일만 anchor
    tile.tileWidth   = width;      // 모든 타일에 저장 (anchor 조회 편의)
    floorData.tiles[anchor.x + i] = tile;
}
```

- anchor: 왼쪽 첫 번째 타일 (`isAnchor=true`, `tileWidth=width`)
- auxiliary: 나머지 타일 (`isAnchor=false`, 동일 `tenantEntity`)

### 5.4 removeTenant() — anchor 자동 탐색

```cpp
Result<bool> GridSystem::removeTenant(TileCoord anyTile) {
    // 1. anyTile의 TileData 조회
    // 2. findAnchor(anyTile)로 anchor 찾기
    // 3. anchor.tileWidth만큼 전체 타일 초기화 (TileData() 기본값)
}
```

- 임의 타일(anchor 또는 auxiliary)로 호출 가능
- `findAnchor()`가 왼쪽 방향 순회로 anchor 위치 반환

### 5.5 findAnchor() 구현

```cpp
std::optional<TileCoord> GridSystem::findAnchor(TileCoord anyTile) const {
    // 왼쪽으로 이동하며 isAnchor=true 타일 탐색
    int x = anyTile.x;
    while (x >= 0) {
        if (currentTile->isAnchor) return pos;
        if (currentTile->tenantEntity != tile->tenantEntity) break;
        --x;
    }
    return std::nullopt;
}
```

- O(width) 시간 복잡도
- 다른 테넌트 경계에서 탐색 중단

### 5.6 이벤트 발행

| 작업 | 발행 이벤트 |
|------|------------|
| buildFloor() | `FloorBuilt` |
| placeTenant() | `TileOccupied` (각 타일) + `TenantPlaced` (anchor) |
| removeTenant() | `TileVacated` (각 타일) + `TenantRemoved` (anchor) |
| placeElevatorShaft() | `TileOccupied` (각 타일) |

---

## 6. 테스트 결과

```
 1/18  GridSystem - 초기 상태                              PASSED
 2/18  GridSystem - buildFloor 성공                        PASSED
 3/18  GridSystem - buildFloor 중복                        PASSED
 4/18  GridSystem - 단일 타일 테넌트 배치                  PASSED
 5/18  GridSystem - 멀티 타일 테넌트 배치 (width=3)        PASSED
 6/18  GridSystem - 미건설 층에 테넌트 배치 → FloorNotBuilt PASSED
 7/18  GridSystem - 범위 초과 테넌트 배치 → OutOfRange     PASSED
 8/18  GridSystem - anchor 타일로 테넌트 제거              PASSED
 9/18  GridSystem - auxiliary 타일로 테넌트 제거           PASSED
10/18  GridSystem - isTileEmpty                            PASSED
11/18  GridSystem - isValidCoord 경계                      PASSED
12/18  GridSystem - getFloorTiles                          PASSED
13/18  GridSystem - 엘리베이터 샤프트 설치                 PASSED
14/18  GridSystem - isElevatorShaft                        PASSED
15/18  GridSystem - 엘리베이터 샤프트에 테넌트 배치 거부   PASSED
16/18  GridSystem - findAnchor                             PASSED
17/18  GridSystem - findAnchor (anchor 자기 자신)          PASSED (16번 내 포함)
18/18  GridSystem - findNearestEmpty                       PASSED

100% tests passed, 0 tests failed out of 18
```

---

## 7. 설계 원칙 준수 여부

| 원칙 | 준수 여부 | 비고 |
|------|-----------|------|
| exception 사용 금지 | ✅ | 모든 실패 → Result<T> 반환 |
| Layer 경계 준수 | ✅ | IGridSystem(L0), GridSystem(L1), 게임 규칙 없음 |
| ConfigManager hot path 호출 금지 | ✅ | 생성자에서 로컬 변수로 캐싱 |
| 멀티타일 anchor 규칙 | ✅ | 왼쪽 타일 anchor, auxiliary 동일 entityId |
| removeTenant 임의 타일 지원 | ✅ | findAnchor()로 자동 탐색 |
| 엘리베이터 샤프트 보호 | ✅ | 샤프트 타일에 테넌트 배치 시 오류 반환 |
| 미건설 층 보호 | ✅ | FloorNotBuilt 에러코드 |
| 층 0 자동 건설 | ✅ | 생성자에서 처리 |

---

## 8. 미결 사항 / 검토 요청 항목

1. **`buildFloor()` 연속 건설 제약**: 현재 구현은 층 순서와 무관하게 아무 층이나 건설 가능 (예: 층 5를 층 1~4 없이 건설 가능). SimTower 원작에서는 아래 층부터 순서대로 건설해야 하는지 확인 필요.

2. **`getFloorTiles()` 반환 범위**: 현재 빈 타일(COUNT)도 포함해서 반환. 점유된 타일만 반환하는 것이 더 효율적인지, 아니면 전체 타일 그리드가 필요한지 (렌더링 레이어 요구사항 확인 필요).

3. **`findNearestEmpty()` 검색 알고리즘**: 현재 Manhattan distance 기반 shell 탐색. 건물 게임 특성상 같은 층 내 수평 이동이 더 자연스러운데, 층 우선(horizontal-first) 탐색이 맞는지 검토 필요.

4. **`tileWidth` 저장 위치**: 현재 anchor 외 auxiliary 타일에도 `tileWidth`를 저장하고 있음. auxiliary 타일에서 width가 필요한 경우가 있는지, 또는 anchor에만 저장해도 충분한지.

5. **층 0 로비 타일 처리**: 현재 층 0은 `built=true`로 초기화되지만 로비 타일(`isLobby=true`) 설정이 없음. 특정 타일을 로비로 지정하는 로직이 필요한지.

---

## 9. Sprint 1 전체 진행 현황

| Task | 제목 | 상태 |
|------|------|------|
| TASK-01-001 | CMake 프로젝트 구조 | ✅ 완료 |
| TASK-01-002 | SimClock + EventBus | ✅ 완료 |
| TASK-01-003 | ConfigManager + JSON 로딩 | ✅ 완료 |
| TASK-01-004 | IGridSystem + GridSystem | ✅ 완료 |
| TASK-01-005 | IAgentSystem + ECS 컴포넌트 | ⏳ 대기 |
| TASK-01-006 | ITransportSystem + ElevatorSystem | ⏳ 대기 |
| TASK-01-007 | IEconomyEngine + EconomyEngine | ⏳ 대기 |
| TASK-01-008 | SDL2 렌더러 초기화 | ⏳ 대기 |
| TASK-01-009 | 타일 렌더링 | ⏳ 대기 |
| TASK-01-010 | 입력 처리 | ⏳ 대기 |

---

*이 보고서는 외부 AI 검증·검토 요청용입니다. 8번 미결 사항 위주로 의견 주시면 됩니다.*
