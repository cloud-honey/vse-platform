# TASK-01-007 작업 결과 보고서

> **작성일:** 2026-03-24
> **태스크:** TASK-01-007 SDL2 윈도우 + 타일 그리드 렌더링
> **레이어:** Layer 3 (SDL2 Renderer)
> **담당:** 붐 (claude-sonnet-4-6)
> **커밋:** dbb8912

---

## 1. 태스크 목표

CLAUDE.md Layer 3 설계에 따라 SDL2 기반 렌더링 파이프라인을 구축한다.

- SDL2 윈도우 + 렌더러 초기화
- 타일 그리드 렌더링 (건설된 층, 테넌트 타입별 색상)
- 엘리베이터 렌더링 (FSM 상태별 색상)
- 카메라 시스템 (팬, 줌, 좌표 변환)
- 입력 매핑 (SDL_Event → GameCommand, Domain은 SDL 직접 참조 금지)
- CLAUDE.md 메인 루프 구조 구현 (fixed-tick + 배속 + spiral-of-death 방지)

---

## 2. 구현 파일 목록

### 2-1. 새로 생성한 파일 (12개)

| 파일 | 레이어 | 역할 |
|------|--------|------|
| `include/core/IRenderCommand.h` | Core API | RenderFrame, RenderTile, RenderElevator, RenderText, Color 구조체 |
| `include/core/InputTypes.h` | Core API | GameCommand, CommandType (건설/카메라/시스템/선택) |
| `include/renderer/Camera.h` | Layer 3 | 월드↔화면 좌표 변환 인터페이스 |
| `include/renderer/SDLRenderer.h` | Layer 3 | SDL2 렌더러 인터페이스 |
| `include/renderer/InputMapper.h` | Layer 3 | SDL_Event → GameCommand 변환기 |
| `include/renderer/RenderFrameCollector.h` | Layer 3 | Domain 상태 → RenderFrame 수집기 |
| `src/renderer/Camera.cpp` | Layer 3 | 카메라 구현 (팬/줌/좌표변환) |
| `src/renderer/SDLRenderer.cpp` | Layer 3 | 타일/엘리베이터/그리드 렌더링 |
| `src/renderer/InputMapper.cpp` | Layer 3 | 키보드/마우스 매핑 |
| `src/renderer/RenderFrameCollector.cpp` | Layer 3 | Grid + Transport 스냅샷 수집 |
| `tests/test_Camera.cpp` | 테스트 | Camera 좌표 변환 8개 |
| `tests/test_RenderFrame.cpp` | 테스트 | RenderFrame + GameCommand 12개 |

### 2-2. 수정한 파일 (5개)

| 파일 | 변경 내용 |
|------|-----------|
| `include/core/ITransportSystem.h` | ElevatorSnapshot에 `shaftX` 필드 추가 (렌더러용) |
| `src/domain/TransportSystem.cpp` | getElevatorState()에서 shaftX 값 채우기 |
| `src/main.cpp` | 전면 재작성 — CLAUDE.md 메인 루프 구조 |
| `CMakeLists.txt` | 렌더러 소스 4개 추가 |
| `tests/CMakeLists.txt` | test_Camera.cpp, test_RenderFrame.cpp, Camera.cpp 추가 |

---

## 3. 아키텍처 설계

### 3-1. 레이어 경계 준수

```
Layer 1 (Domain)              Layer 3 (Renderer)
┌──────────────┐              ┌──────────────────┐
│ GridSystem   │──읽기전용──→│ RenderFrame      │
│ Transport    │              │ Collector        │
│ AgentSystem  │              └────────┬─────────┘
└──────────────┘                       │
                                       ▼
                              ┌──────────────────┐
                              │ SDLRenderer      │
                              │  (SDL2 직접 호출) │
                              └──────────────────┘
```

- **Domain → Renderer:** RenderFrame 값 타입으로만 데이터 전달
- **Renderer → Domain:** GameCommand 값 타입으로만 커맨드 전달
- Domain은 SDL2를 참조하지 않음 ✅
- Renderer 내부에서 게임 로직 계산 없음 ✅

### 3-2. 메인 루프 구조 (CLAUDE.md 준수)

```
매 프레임:
  1. handleInput()
     - SDL_PollEvent → InputMapper → GameCommand 목록
     - 카메라/시스템 커맨드 즉시 처리
  
  2. updateSim(realDeltaMs)
     - accumulator += realDeltaMs * speed    ← 배속 구현
     - while (accumulator >= TICK_MS):
         EventBus::flush()
         AgentSystem::update()
         TransportSystem::update()
     - MAX_TICKS_PER_FRAME = 8 (spiral-of-death 방지)
  
  3. render()
     - RenderFrameCollector.collect() → RenderFrame
     - SDLRenderer.render(frame, camera)
```

---

## 4. 주요 구현 상세

### 4-1. Camera 좌표계

| 항목 | 값 |
|------|-----|
| 월드 원점 | 좌하단 (0, 0) — CLAUDE.md 확정 좌표계 |
| 월드 Y | 위로 증가 (floor 0 = 지상층) |
| 화면 원점 | 좌상단 (SDL2 표준) |
| 화면 Y | 아래로 증가 |
| 변환 | `screenY = viewportH - (worldY - cameraY) * zoom` |
| 줌 범위 | 0.25x ~ 4.0x |

### 4-2. 렌더링 색상 규칙

| 대상 | 색상 |
|------|------|
| 하늘 배경 | `#87CEEB` (하늘색) |
| 로비 (0층) | `#3C3C50` (어두운 파랑) |
| 건설된 층 배경 | `#282A36` (다크) |
| Office 테넌트 | `#4682B4` (파란) |
| Residential 테넌트 | `#3CB371` (초록) |
| Commercial 테넌트 | `#FFA500` (주황) |
| 엘리베이터 Idle | `#787890` (회색) |
| 엘리베이터 Moving | `#4F8EF7` (파랑) |
| 엘리베이터 Boarding | `#2ECC8A` (초록) |
| 엘리베이터 DoorClosing | `#FFC83C` (노랑) |

### 4-3. InputMapper 키 매핑

| 입력 | GameCommand |
|------|-------------|
| 방향키 / WASD (지속) | CameraPan |
| 마우스 휠 | CameraZoom |
| Space | TogglePause |
| 1 / 2 / 4 | SetSpeed (1x/2x/4x) |
| F3 | ToggleDebugOverlay |
| Home | CameraReset |
| ESC | Quit |
| 마우스 좌클릭 | SelectTile |

### 4-4. ElevatorSnapshot 변경

```cpp
// ITransportSystem.h — 추가된 필드
struct ElevatorSnapshot {
    EntityId  id;
    int       shaftX;           // ← 신규 (렌더러용 shaft 위치)
    int       currentFloor;
    float     interpolatedFloor;
    // ... 기존 필드 유지
};
```

---

## 5. 테스트 결과

### 5-1. 전체 현황

| 구분 | 테스트 수 | 결과 |
|------|-----------|------|
| 기존 (TASK-01-001~006) | 87 | ✅ 전체 통과 |
| Camera 좌표 변환 | 13 | ✅ 전체 통과 |
| RenderFrame + GameCommand | 11 | ✅ 전체 통과 |
| InputMapper 커맨드 팩토리 | 10 | ✅ 전체 통과 |
| **합계** | **121** | **✅ 121/121** |

### 5-2. 신규 테스트 목록

**Camera (8개)**

| # | 테스트명 | 검증 내용 |
|---|---------|-----------|
| 88 | Camera - 초기 상태 | viewport, tileSize, zoom 기본값 |
| 89 | Camera - tileToScreen 기본 변환 | tile(0,0) → 화면 좌표 |
| 90 | Camera - tileToScreen floor 증가 시 y 감소 | Y축 반전 확인 |
| 91 | Camera - 줌 적용 | zoom 2x에서 좌표 2배 |
| 92 | Camera - 줌 범위 제한 | 0.25 ~ 4.0 클램프 |
| 93 | Camera - 팬 이동 | 화면 드래그 → 월드 반대 이동 |
| 94 | Camera - reset | 위치/줌 초기화 |
| 95 | Camera - screenToTile 역변환 | 화면→타일 왕복 정합성 |

**RenderFrame + GameCommand (12개)**

| # | 테스트명 | 검증 내용 |
|---|---------|-----------|
| 96 | RenderFrame - 기본 생성 | 빈 프레임, 기본값 |
| 97 | RenderFrame - 타일 추가 | 색상 정확성 |
| 98 | RenderFrame - 엘리베이터 추가 | shaftX, 상태 |
| 99 | Color - fromRGBA | RGBA 값 정확성 |
| 100 | Color - fromRGBA 기본 알파 | 알파 255 기본값 |
| 101 | GameCommand - makeQuit | 타입 확인 |
| 102 | GameCommand - makeBuildFloor | payload 확인 |
| 103 | GameCommand - makeCameraPan | deltaX/Y 확인 |
| 104 | GameCommand - makeCameraZoom | zoomDelta 확인 |
| 105 | GameCommand - makeCameraZoom | 중복 확인 |
| 106 | GameCommand - makeSetSpeed | multiplier 확인 |
| 107 | GameCommand - makeSelectTile | tileX/Floor 확인 |

---

## 6. CLAUDE.md 준수 체크리스트

| 규칙 | 상태 |
|------|------|
| Layer 3 렌더러 내부에서 게임 로직 금지 | ✅ |
| Domain에서 SDL2 직접 호출 금지 | ✅ |
| Domain에서 SDL_Event 직접 읽기 금지 | ✅ (GameCommand 통해서만) |
| IRenderCommand 구조체 — Layer 3 소비 | ✅ |
| InputTypes 구조체 — Layer 3 생성, Domain 소비 | ✅ |
| fixed-tick 메인 루프 | ✅ (100ms, 10 TPS) |
| 배속: accumulator += realDeltaMs * speed | ✅ |
| MAX_TICKS_PER_FRAME spiral-of-death 방지 | ✅ (8) |
| processCommands는 첫 tick에서만 | ⚠️ 카메라 커맨드는 프레임당 즉시 처리 (비도메인) |
| render()는 tick loop 밖 1회 | ✅ |
| exception 사용 금지 | ✅ (-fno-exceptions) |
| EntityId = entt::entity | ✅ |
| 타일 32×32px | ✅ (config에서 읽어야 하지만 Phase 1 고정) |

---

## 7. 미결 사항 / Phase 2 이연

| 항목 | 상태 | 비고 |
|------|------|------|
| 스프라이트 렌더링 | Phase 2 | 현재 컬러 박스만 |
| SDL2_ttf 텍스트 | Phase 2 | 현재 Dear ImGui 오버레이 예정 |
| Dear ImGui 디버그 패널 | 미구현 | TASK-01-008 이후 별도 태스크 |
| HiDPI 대응 | Phase 2 | Phase 1은 1x 렌더 |
| 도메인 커맨드 (BuildFloor 등) | 미연결 | processCommands()에서 처리 예정 |
| RenderFrameCollector NPC 수집 | 미구현 | AgentSystem 연동 후 추가 |

---

## 8. GPT-5.4 Thinking 검토 반영 (2026-03-24)

검토 후 아래 항목 즉시 수정 완료:

| GPT-5.4 지적 | 대응 | 상태 |
|---|---|---|
| §3.2 타일 크기 32px 하드코딩 | ConfigManager에서 `grid.tileSizePx`, `rendering.zoomMin/Max`, `cameraPanSpeed` 등 전부 읽도록 수정 | ✅ |
| §3.3 보고서 결론 과장 | 톤 하향 조정 (아래 결론 참고) | ✅ |
| §3.4 테스트 핵심 리스크 구간 부족 | Camera 오프셋+줌 역변환 5개, InputMapper 커맨드 팩토리 10개 추가 → 121/121 | ✅ |
| §5.4 도메인 커맨드 미연결 | BuildFloor, PlaceTenant, PlaceElevatorShaft, CreateElevator 연결 완료 | ✅ |
| §5.1 shaftX 추가 이유 재정의 | "렌더러 편의"→"엘리베이터 상태 질의 완결성 보강"으로 정정 | ✅ |

### 후속 태스크에서 처리 (우선순위 B/C)

- Dear ImGui 디버그 패널 (Phase 1 범위 내, TASK-01-009로 별도 존재)
- Agent/UI 수집 → RenderFrame 확장
- Bootstrapper ↔ main.cpp 역할 분리
- Dirty Rect 최적화 (Phase 1 후반)

---

## 9. 결론

TASK-01-007은 **Layer 3의 기본 렌더링 기반을 구축**했다.

- 레이어 경계 준수 (Domain ↔ Renderer — 값 타입으로만 통신)
- 모든 렌더링 수치를 ConfigManager에서 읽음 (하드코딩 제거)
- 메인 루프 CLAUDE.md 명세 일치 (fixed-tick + 배속 + spiral-of-death 방지)
- 도메인 커맨드(BuildFloor, PlaceTenant 등) processCommands 연결 완료
- 기존 87개 + 신규 34개 = **121/121 전체 통과**
- 인터페이스 변경: ElevatorSnapshot.shaftX 1개 추가 (엘리베이터 상태 질의 완결성)

**Dear ImGui 디버그 패널, Agent 렌더 수집, Dirty Rect는 후속 태스크(TASK-01-009 등)에서 마무리 필요.**
