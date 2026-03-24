# CLAUDE.md — VSE Platform Phase 1 구현 사양서
> Version: 1.3 | 대상: 붐(PM) + DeepSeek(개발) + Claude Code(렌더링)
> 이 파일이 모든 구현 판단의 최우선 기준이다. 여기 없는 것은 Human에게 먼저 묻는다.

---

## 프로젝트 개요

**Tower Tycoon** — SimTower 스타일 빌딩 관리 시뮬레이션
- 엔진: C++17 + SDL2 + Dear ImGui + EnTT
- 빌드: CMake
- 배포: macOS + Steam (Phase 1 한정)
- 목표: Steam 출시 가능한 MVP

---

## Phase 1 절대 범위 (이것만 만든다)

### 포함
- 타일 그리드 (30층 이하)
- NPC AI (시간대별 이동 패턴, 최대 50명)
- 엘리베이터 시뮬 (1축 2~4대, LOOK 알고리즘)
- 테넌트 3종 (사무실 / 주거 / 상업)
- 기본 경제 (수입 / 지출)
- 별점 시스템 (단순화)
- SDL2 렌더링 + Dear ImGui 디버그 툴
- macOS + Steam 빌드

### 제외 (코드 한 줄도 없어야 함)
- BAS / BACnet / IIoTAdapter
- 멀티플레이어 / INetworkAdapter 구현
- 재난 시스템
- iOS / Android 빌드
- VariableEncryption
- AuditLogger (spdlog로 대체)
- Destination Dispatch (v1.1)
- 부채 / 이자 / 인플레이션

### 설계만 있고 구현 없는 것 (슬롯 확보용)
- INetworkAdapter + IAuthority (인터페이스 선언, 메서드 비워둠)
- SpatialPartitioning (인터페이스 선언만, 구현은 성능 실측 후)

---

## 아키텍처 레이어

```
Layer 0  VSE Core
  ├─ Core API      include/core/   인터페이스(I*), 공유 타입, 열거형. 구체 구현 금지.
  └─ Core Runtime  src/core/       공용 인프라(SimClock, EventBus, ConfigManager 등).
                                   게임 규칙(만족도 계산, 임대료 공식 등) 금지.
Layer 1  Domain Module  RealEstateDomain. 게임 규칙은 여기에만.
Layer 2  Content Pack   JSON + 스프라이트. 코드 변경 없이 DLC 가능.
Layer 3  SDL2 Renderer  IRenderCommand/GameCommand 기반. Input → GameCommand → Domain.
```

**레이어 경계 위반 예시 (금지):**
- Layer 3 렌더러 내부에서 게임 로직 계산 → 금지
- Core API(include/core/)에 구체 구현 포함 → 금지
- Core Runtime(src/core/)에 게임 규칙 포함 → 금지 (예: 만족도 공식, 임대료 계산)
- Layer 1 도메인에서 SDL2 직접 호출 → 금지
- Layer 1 도메인에서 SDL_Event 직접 읽기 → 금지 (GameCommand 통해서만)

---

## Layer 0 모듈 목록 (Phase 1 구현 기준)

### Core API (include/core/ — 인터페이스·타입만)

| 모듈 | 구현 여부 | 비고 |
|---|---|---|
| IGridSystem | ✅ 인터페이스 | 구현은 Layer 1 domain/ |
| IAgentSystem | ✅ 인터페이스 | 구현은 Layer 1 domain/ |
| ITransportSystem | ✅ 인터페이스 | 구현은 Layer 1 domain/ |
| IEconomyEngine | ✅ 인터페이스 | 구현은 Layer 1 domain/ |
| ISaveLoad | ✅ 인터페이스 | 구현은 Layer 1 domain/ |
| IAsyncLoader | ✅ 인터페이스 | 구현은 src/core/AsyncLoader.cpp |
| IMemoryPool | ✅ 인터페이스 | 구현은 src/core/MemoryPool.cpp |
| IRenderCommand | ✅ 구조체 | RenderFrame 등 — Layer 3 소비 |
| IAudioCommand | ✅ 구조체 | BGM/SFX 커맨드 |
| InputTypes | ✅ 구조체 | GameCommand — Layer 3 생성, Domain 소비 |
| Types.h / Error.h | ✅ 공유타입 | EntityId = entt::entity |
| SpatialPartitioning | ⚠️ 선언만 | 성능 실측 후 구현 결정 |
| INetworkAdapter + IAuthority | ⚠️ 선언만 | Phase 3. 메서드 비워둠. |

### Core Runtime (src/core/ — 공용 인프라, 게임 규칙 금지)

| 모듈 | 구현 여부 | 비고 |
|---|---|---|
| EventBus | ✅ 구현 | 단일 버스 + replicable 플래그. **Deferred-only.** |
| SimClock | ✅ 구현 | 100ms 고정 틱. 1 tick = 1 game minute. |
| ConfigManager | ✅ 구현 | game_config.json 로드 |
| ContentRegistry | ✅ 구현 | 핫 리로드 포함 |
| LocaleManager | ✅ 구현 | ko / en 우선 |
| Bootstrapper | ✅ 구현 | Composition root — 시스템 조립·초기화 순서 관리. 장기적으로 인터페이스 기반 조립으로 전환 예정. |
| AsyncLoader | ✅ 구현 | IAsyncLoader 구현체 |
| MemoryPool | ✅ 구현 | IMemoryPool 구현체 |

### 존재하지 않는 것

| 모듈 | 비고 |
|---|---|
| IIoTAdapter | Phase 2. 슬롯 주석만 허용. |
| ISpatialRule | Phase 2+. |
| AuditLogger | spdlog 사용. |
| VariableEncryption | Steam 출시 후 재검토. |

---

## 시뮬레이션 설정

| 항목 | 값 | 비고 |
|---|---|---|
| SimClock Tick | 100ms (10 TPS) | 고정. 변경 시 Human 승인 필요. |
| Tick → GameTime | 1 tick = 1 game minute | 1440 ticks/day. Phase 1 고정. |
| 렌더링 | 60fps 목표 | 시뮬과 독립 루프 |
| NPC 최대 | 50명 | Phase 1 하드 상한 |
| 층수 최대 | 30층 | Phase 1 하드 상한 |
| 엘리베이터 | 1축 2~4대 | LOOK 알고리즘 |
| 테넌트 종류 | 3종 | 사무실 / 주거 / 상업 |
| 타겟 프레임 | 60fps | 최소 30fps 유지 |
| 모바일 메모리 | 해당 없음 | Phase 1은 macOS만 |

### 메인 루프 규칙 (fixed-tick loop — 확정)

```
매 프레임:
  1. handleInput(): SDL_PollEvent → InputMapper → commandQueue
  2. updateSim(realDeltaMs):
     accumulator += realDeltaMs * speed   ← speed 곱해서 배속 구현
     ticksThisFrame = 0
     while (accumulator >= TICK_MS && ticksThisFrame < MAX_TICKS_PER_FRAME):
       accumulator -= TICK_MS
       ticksThisFrame++
       EventBus::flush()                  ← tick N-1 이벤트 배달
       SimClock::advanceTick()            ← tick N 시작, TickAdvanced 발행(→N+1로 큐잉)
       if (ticksThisFrame == 1):
         processCommands()                ← 첫 tick에서만 drain (프레임당 1회)
       AgentSystem::update()
       TransportSystem::update()
       EconomyEngine::update()
       StarRatingSystem::update()
       ContentRegistry::checkAndReload()  ← N ticks마다
  3. render(): RenderFrame 수집 → SDLRenderer
```

- **system update는 frame 기준이 아니라 tick 기준으로 돈다**
- **배속 원리:** `accumulator += realDeltaMs * speed`. 2x면 accumulator가 2배 빠르게 차서 초당 20 tick, 4x면 40 tick.
- **MAX_TICKS_PER_FRAME:** spiral-of-death 방지 상한. `speed * 2` 정도 (예: 4x → 상한 8). 이 이상은 프레임 드랍 허용.
- **processCommands():** commandQueue_를 **drain(전부 소비 후 비움)**. 1프레임 내 첫 tick에서만 실행 — 이후 tick은 빈 큐라 no-op.
- render()는 tick loop 밖에서 1 프레임당 1회만 호출

### config vs 고정값 정책

| 구분 | 규칙 |
|---|---|
| `defaults::` (Types.h) | JSON 로드 실패 시에만 쓰는 fallback. 코드에서 직접 참조 금지. |
| `game_config.json` | 런타임 값의 유일한 출처. ConfigManager 통해서만 접근. |
| `balance.json` | 핫 리로드 가능. ContentRegistry 통해서만 접근. |
| `tickMs`, `ticksPerGameMinute` | **Phase 1에서는 config에 기록만, 런타임 변경 불가.** config 값이 고정값(100, 1)과 다르면 초기화 시 경고 로그 출력 후 고정값 사용. |

---

## ECS 설정 (EnTT)

- 라이브러리: EnTT (Header-only, C++17)
- 커스텀 ECS 구현 금지
- 컴포넌트는 데이터만, 로직은 System에
- 엔티티 생명주기는 System 단위로 관리

```cpp
// 컴포넌트 예시 — 데이터만
struct PositionComponent { int x; int floor; };
struct AgentComponent { AgentRole role; NeedsState needs; EntityId currentBuilding; };

// System 예시 — fixed-tick 기반, dt 없음
// All gameplay systems update on fixed simulation ticks, not variable frame delta.
class AgentSystem {
    void update(entt::registry& reg, const GameTime& time);
};
```

---

## EventBus 사용 규칙

```cpp
// Phase 1: 단일 EventBus + replicable 플래그 + deferred-only
struct Event {
    EventType type;
    bool replicable = false;  // Phase 3 네트워크 동기화 대상 여부
    EntityId source;
    std::any payload;
};
```

- **Deferred-only:** tick N에서 publish() → tick N+1 시작 시 flush()에서 배달. 즉시 배달 금지.
- 모든 이벤트는 타입 선언 시 replicable 여부 명시
- UI/애니메이션 이벤트는 replicable=false 기본
- 경제/상태 변경 이벤트는 replicable=true 기본
- flush() 중 새로 publish된 이벤트는 다음 tick에 배달 (double buffer)

---

## 직렬화 (MessagePack)

- 세이브 파일 포맷: MessagePack
- 세이브 파일에 버전 번호 포함 (현재: v1)
- Migration 로직은 Phase 2에서 추가
- FlatBuffers 사용 금지 (Phase 1)

```cpp
// 버전 헤더 예시
struct SaveHeader {
    uint32_t version = 1;
    // ...
};
```

### Entity 저장/복원 방식 (확정)
- **Phase 1: EnTT snapshot 기반** (`entt::snapshot` / `entt::snapshot_loader`) — entity + component 저장용
- entity ID는 EnTT가 내부적으로 재매핑 (별도 stable ID 불필요)
- 엔티티 간 참조 (passengers, homeTenant, workplace)는 같은 snapshot 내에서만 자동 안전 — **SaveLoad 테스트에서 반드시 round-trip 검증**
- **Non-ECS 시스템 내부 상태** (GridSystem floors_, Economy balance/records, StarRating)는 **시스템별 커스텀 직렬화**로 별도 처리
- **복원 순서**: (1) snapshot_loader로 전체 ECS 복원 → (2) non-ECS 상태 복원 → (3) derived cache 재계산 (상세: VSE_Design_Spec.md §5.8)
- Phase 2: save format migration + stable ID 검토

---

## 메모리 소유권 컨벤션

| 상황 | 사용 |
|---|---|
| 단일 소유자 명확 | `unique_ptr` |
| 공유 필요 (최소화) | `shared_ptr` |
| 소유권 없는 참조 | raw pointer 또는 reference |
| 컨테이너 내 폴리모피즘 | `unique_ptr<IBase>` |

- `new` / `delete` 직접 사용 금지
- EnTT registry가 관리하는 컴포넌트는 raw pointer 허용

---

## 에러 핸들링 정책 ✅ 확정

| 상황 | 방법 |
|---|---|
| 개발 중 불변 조건 위반 | `assert()` — 릴리즈 빌드에서 제거됨 |
| 런타임 복구 가능 오류 | `std::optional` 또는 error code 반환 |
| 런타임 복구 불가 오류 | spdlog로 기록 후 graceful shutdown |
| 예외(exception) | **사용 금지** |

```cpp
// CORRECT: error code return
std::optional<TileCoordinate> GridSystem::getTile(int x, int floor) {
    if (floor < 0 || floor >= MAX_FLOORS) return std::nullopt;
    return TileCoordinate{x, floor};
}

// WRONG: never do this
// throw std::out_of_range("floor out of bounds"); // FORBIDDEN
```

---

## 네이밍 컨벤션

| 대상 | 규칙 | 예시 |
|---|---|---|
| 인터페이스 | `I` 접두사 + PascalCase | `IGridSystem` |
| 클래스 | PascalCase | `ElevatorController` |
| 함수 | camelCase | `updatePosition()` |
| 변수 | camelCase | `currentFloor` |
| 상수 | UPPER_SNAKE | `MAX_NPC_COUNT` |
| 파일 | PascalCase | `GridSystem.cpp` |
| 컴포넌트 | `Component` 접미사 | `PositionComponent` |
| 시스템 | `System` 접미사 | `AgentMovementSystem` |

---

## 하드코딩 금지 목록

다음 수치는 반드시 외부 JSON / ConfigManager에서 읽어야 한다:

- NPC 이동 속도, 만족도 증감 수치
- 엘리베이터 속도, 용량
- 테넌트 임대료, 유지비
- 별점 기준 수치
- 타임 스케일 배속 값
- 층당 타일 수, 타일 크기(px)

---

## 타일 그리드 좌표계 ✅ 확정

| 항목 | 값 |
|---|---|
| 1타일 크기 | 32 × 32 px |
| 층고 | 32 px (1타일 = 1층) |
| 원점 | 좌하단 (0, 0) |
| Y축 방향 | 위로 증가 (floor 0 = 지상층) |
| 좌표계 기준 | SimTower 기준 + 픽셀아트 표준 |

```cpp
// DESIGN DECISION: tile = 32x32px, origin = bottom-left, Y increases upward.
// Matches SimTower convention. See VSE_Architecture_Review.md.
struct TileCoordinate { int x; int floor; }; // floor 0 = ground
```

---

## 스프라이트 / 아트 스타일 ✅ 확정

| 항목 | 값 |
|---|---|
| 스타일 | 픽셀아트 (SimTower 느낌) |
| 타일 해상도 | 32 × 32 px |
| NPC 스프라이트 | 16 × 32 px (타일 너비의 절반, 높이 1층) |
| AI 생성 기준 | 32px 그리드 기반, 제한 팔레트 권장 (16~32색) |
| 렌더 배율 | 1x (논리 px = 화면 px), HiDPI 대응은 Phase 2 |

---

## CI/CD 정책 ✅ 확정

- **도구:** GitHub Actions
- **트리거:** main/dev 브랜치 push 시 자동 실행
- **최소 체크:** macOS 빌드 성공 여부
- **설정 파일:** `.github/workflows/build.yml` (Phase 1 시작 시 생성)

---

## 라이선스

> 검토 중
- Layer 0: MIT 라이선스 (오픈소스 배포 가능)
- Layer 1+: 상업 라이선스
- 결정 후 이 항목 업데이트 필요

---

## Phase 2 슬롯 (코드 주석으로만 존재)

```cpp
// [PHASE-2] IIoTAdapter: BACnet/IP 연동 예정
// 스레드 격리 + 비동기 큐 패턴 사용 예정
// MockBACnetDevice로 먼저 테스트 후 실장비 연동
```

```cpp
// [PHASE-3] INetworkAdapter: 권위 서버 모델 예정
// IAuthority + IReplicationManager 계층 추가 예정
```

---

## 붐(PM)이 구현 지시 시 필수 포함 항목

DeepSeek / Claude Code에게 태스크를 줄 때 반드시 포함:

1. 구현 대상 레이어 명시 (Layer 0 / 1 / 2 / 3)
2. 관련 인터페이스명
3. Phase 1 범위 내 여부 확인
4. 단위 테스트 생성 요청 포함
5. 네이밍 컨벤션 준수 요청

---

## 변경 이력

| 날짜 | 버전 | 내용 |
|---|---|---|
| 2026-03 | 1.0 | 크로스 검증 완료 후 최초 작성. |
| 2026-03 | 1.1 | 미결 4개 전부 확정. 타일 좌표계·스프라이트·CI/CD·에러핸들링 추가. |
| 2026-03-24 | 1.2 | Layer 0 = Core API + Core Runtime 분리 정의. EventBus deferred-only 규칙 추가. fixed-tick loop 확정. config vs 고정값 정책. EnTT snapshot 기반 저장/복원. Bootstrapper = composition root 명시. GameCommand 입력 흐름 추가. |
| 2026-03-24 | 1.3 | External review (DeepSeek R1 + GPT-5.4) 반영: spawnAgent() EntityId 기반 재설계, SaveLoad 참조 안전성 테스트 의무화, ECS 예시 최신화, PixelPos x 변환식, 버전 동기화. |

---
*이 파일의 내용을 변경하려면 반드시 Human 승인 후 붐(PM)이 업데이트한다.*
