# VSE 설계 문서 수정 이력

> 마지막 업데이트: 2026-03-24 15:18 KST
> 현재 최신 커밋: `848b416` (main)

---

## 커밋 타임라인

| 순서 | 커밋 | 시간 | 제목 |
|------|------|------|------|
| 1 | `d4a6a73` | 03-24 10:05 | init: VSE Platform project setup |
| 2 | `4e89b2c` | 03-24 11:30 | sprint-1: task breakdown + dashboard |
| 3 | `7c3a7e6` | 03-24 12:15 | [TASK-01-001] docs: CMake task spec |
| 4 | `aa230f6` | 03-24 13:42 | design: Phase 1 design spec 완성 |
| 5 | `a910523` | 03-24 14:01 | design v1.1: 11개 개선사항 |
| 6 | `46c2b73` | 03-24 14:26 | design v1.2: 6개 최종 리뷰 |
| 7 | `7885d07` | 03-24 14:37 | design v1.3: 자체 리뷰 9개 수정 |
| 8 | `848b416` | 03-24 14:57 | docs: 잔여 수정 3개 마무리 |

---

## 문서별 수정 이력

### 📄 VSE_Design_Spec.md

현재 버전: **v1.3** | 최초 생성: `aa230f6` | 최신 수정: `848b416`

#### v1.0 — 최초 작성 (`aa230f6`, 13:42)
- 1,433줄 신규 작성
- 14개 인터페이스 정의 (ISimClock, IEventBus, IGridSystem, IAgentSystem 등)
- 9개 ECS 컴포넌트 (PositionComponent, AgentComponent 등)
- 시스템 업데이트 순서 정의
- JSON 스키마 (game_config.json, balance.json)
- 의존성 그래프 + 초기화 순서

#### v1.1 — 교차 리뷰 11개 개선 (`a910523`, 14:01)
- **Layer 0 분리**: Core API (인터페이스) + Core Runtime (인프라 구현)
- **EntityId**: `entt::entity` 직접 사용으로 확정
- **시간 체계**: 1 tick = 1 게임 분 (하루 1,440틱)
- **EventBus**: deferred-only 확정 (동기 dispatch 금지)
- **GameCommand**: 입력 → 커맨드 큐 → tick에서 처리 흐름
- **Grid**: anchor-tile 기반 점유 시스템
- **엘리베이터**: 6-state FSM 정의
- **SaveLoad**: 복원 순서 명시
- **defaults:: 네임스페이스**: 폴백 전용, ConfigManager가 런타임 소스

#### v1.2 — 최종 리뷰 6개 (`46c2b73`, 14:26)
- **True fixed-tick loop**: accumulator 패턴, `maxTicksPerFrame` = speed × 2
- **EnTT snapshot**: 영속성 방식 확정 (snapshot → MessagePack)
- **Bootstrapper**: composition root 역할 확정
- **tickMs/ticksPerGameMinute**: Phase 1 불변값
- **Domain JSON 직접 읽기 금지**: ContentRegistry 경유 필수
- **Hot-reload**: 600틱마다 balance.json 체크
- 추가 파일 생성: CMakeLists.txt, FetchDependencies.cmake, main.cpp, game_config.json

#### v1.3 — 자체 리뷰 9개 수정 (`7885d07`, 14:37)
**Critical (2개):**
- C-1: 배속 메커니즘 — `accumulator += dt * speed` (틱 캡 방식 폐기)
- C-2: accumulator 소유권 — Bootstrapper로 이전, SimClock은 순수 틱 카운터

**Important (4개):**
- I-1: Core API 헤더 규칙 — `I*.h` = 순수 가상, 나머지 = 런타임 선언
- I-2: processCommands() — 프레임당 첫 틱에서만 1회 drain
- I-3: update() 시그니처 통일 — `(registry&, GameTime&)`, float dt 제거
- I-4: SaveLoad — 비-ECS 상태(Grid floors_, Economy balance) 커스텀 직렬화 필수

**Minor (3개):**
- PixelPos config 참조 추가
- GameCommandIssued = 처리 후 알림 이벤트
- TileCoord에 std::hash 추가

#### 잔여 수정 (`848b416`, 14:57)
- §6.3 시스템 업데이트 순서: `SimClock::update()` → `simClock_->advanceTick()` 표기 통일
- processCommands() 위치 명시 (첫 틱에서만)
- render() 루프 밖 명시 (`--- outside tick loop ---`)

---

### 📄 CLAUDE.md

현재 버전: **v1.2** | 최초: 프로젝트 초기 | 최신 수정: `848b416`

#### 초기 상태 (`d4a6a73`)
- 프로젝트 규칙, 네이밍 컨벤션, 레이어 정의, Phase 1 범위

#### v1.1 (`a910523`, 14:01)
- Design Spec v1.1과 동기화 (Layer 0 분리 반영)

#### v1.2 (`46c2b73`, 14:26)
- fixed-tick loop 규칙 추가
- Bootstrapper 역할 명시
- EnTT snapshot 규칙 추가
- Config 불변값 규칙 추가
- Domain JSON 직접 읽기 금지 규칙

#### v1.2 보강 (`7885d07`, 14:37)
- accumulator 소유권 Bootstrapper 명시
- speed 메커니즘 문구 동기화
- update() 시그니처 `GameTime&` 통일 반영

#### 잔여 수정 (`848b416`, 14:57)
- ECS 예시 코드 최신화:
  - `AgentType` → `AgentRole`
  - `float satisfaction` → `NeedsState needs; EntityId currentBuilding`
  - `void update(entt::registry& reg, float dt)` → `void update(entt::registry& reg, const GameTime& time)`
  - 주석: "All gameplay systems update on fixed simulation ticks, not variable frame delta."

---

### 📄 VSE_AI_Team.md

현재 버전: **v2.1** | 최신 수정: `848b416`

#### 잔여 수정 (`848b416`, 14:57)
- §설계 정합성 체크리스트:
  - `Layer 0에 구현체 없음` → `Core API에 concrete 구현 없음, Core Runtime에 게임 규칙 없음`
- §DeepSeek 절대 규칙:
  - `Layer 0 인터페이스 직접 구현 금지` → `Core API = 인터페이스/POD만, Core Runtime = 게임 규칙 금지, Domain 로직은 Layer 1`

---

### 📄 VSE_Sprint_Status.json

#### 스프린트 1 등록 (`4e89b2c`, 11:30)
- TASK-01-001 ~ TASK-01-010 등록 (전부 status: pending)

#### design_check 업데이트 (`aa230f6`, 13:42)
- 전 태스크 design_check: "done"으로 갱신

---

### 📄 기타 파일

| 파일 | 커밋 | 내용 |
|------|------|------|
| VSE_Dashboard.html | `4e89b2c` | 대시보드 UI (Sprint 상태 표시) |
| VSE_Session_Summary.md | `4e89b2c` | 세션 요약 문서 |
| VSE_Overview.md | `d4a6a73` | 프로젝트 개요 |
| tasks/TASK-01-001.md | `7c3a7e6` | CMake 구조 태스크 명세 |
| CMakeLists.txt | `46c2b73` | 루트 CMake 설정 |
| cmake/FetchDependencies.cmake | `46c2b73` | EnTT, SDL2, nlohmann 등 의존성 |
| src/main.cpp | `46c2b73` | 엔트리포인트 스켈레톤 |
| tests/CMakeLists.txt | `46c2b73` | 테스트 CMake |
| assets/config/game_config.json | `46c2b73` | 게임 설정 기본값 |

---

## 미반영 검토 사항 (대기 중)

아래는 DeepSeek R1 + GPT-5.4 검토에서 나온 피드백으로, 마스터 컨펌 후 반영 예정:

| # | 항목 | 판정 | 상태 |
|---|------|------|------|
| 3.1 | processCommands() 위치 | 보류 | 현행 유지 (문서 명확화로 충분) |
| 3.2 | 엔티티 직렬화 참조 안전성 | 수정 채택 | ⏳ 반영 대기 |
| 3.3 | spawnAgent() 시그니처 | 수정 채택 | ⏳ 반영 대기 |
| 3.4 | FSM/LOOK 분리 가이드 | 채택 | ⏳ 반영 대기 |
| 3.5 | PixelPos x 변환식 | 채택 | ⏳ 반영 대기 |
| 3.6 | 버전 동기화 (CLAUDE.md v1.3) | 채택 | ⏳ 반영 대기 |

---

## 복기용 요약

1. **Design Spec**은 v1.0 → v1.1 → v1.2 → v1.3 순으로 4차례 주요 업그레이드
2. 매 버전마다 교차 검토 → 수정 → 커밋 사이클
3. v1.1이 가장 큰 구조 변경 (11개), v1.3이 가장 중요한 수정 (배속 메커니즘, accumulator 소유권)
4. CLAUDE.md는 Design Spec과 동기화 유지하며 업데이트
5. AI_Team 문서는 Layer 0 체크리스트 정합성 수정 1회
6. 현재 5개 추가 반영 사항 대기 중 (DeepSeek R1 + GPT-5.4 리뷰)
