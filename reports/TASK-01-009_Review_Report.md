# TASK-01-009 작업 결과 보고서

> **작성일:** 2026-03-25
> **태스크:** TASK-01-009 Dear ImGui 디버그 패널 (시간, NPC 상태)
> **레이어:** Layer 3 (SDL2 Renderer)
> **담당:** 붐2 (deepseek/deepseek-chat) + 붐 검토
> **커밋:** 2c3da69
> **테스트:** 142/142 통과 (기존 134 + 신규 8)

---

## 1. 태스크 목표

SDL2 렌더러에 Dear ImGui 디버그 패널을 추가하여 런타임 시뮬레이션 상태를 화면에 오버레이로 표시한다.

**구체 목표:**
- `DebugInfo` 구조체 정의 (시간, NPC 상태, 성능 수치)
- `SDLRenderer`에 ImGui 초기화/종료/렌더 통합
- `InputMapper`에 ImGui 이벤트 우선 처리
- 단위 테스트 8개 작성
- Layer 3에만 ImGui 코드 존재 (레이어 경계 준수)

---

## 2. 구현 파일 목록

### 2-1. 수정한 파일 (4개)

| 파일 | 변경 내용 |
|------|-----------|
| `include/core/IRenderCommand.h` | `DebugInfo` 구조체 추가, `RenderFrame.debug` 필드 추가 |
| `include/renderer/SDLRenderer.h` | `drawDebugPanel()` private 선언 추가 |
| `src/renderer/SDLRenderer.cpp` | ImGui 헤더, 초기화/종료/렌더 통합, `drawDebugPanel()` 구현 |
| `src/renderer/InputMapper.cpp` | ImGui 이벤트 처리 + `WantCapture*` 입력 차단 |

### 2-2. 새로 생성한 파일 (1개)

| 파일 | 역할 |
|------|------|
| `tests/test_DebugInfo.cpp` | DebugInfo 구조체 + RenderFrame 통합 테스트 (8개) |

### 2-3. 수정한 빌드 파일 (1개)

| 파일 | 변경 내용 |
|------|-----------|
| `tests/CMakeLists.txt` | `test_DebugInfo.cpp` 추가 |

---

## 3. 아키텍처 설계

### 3-1. 데이터 흐름

```
main.cpp (Bootstrapper)
│  매 프레임 DebugInfo 갱신:
│    frame.debug.gameTick = clock.currentTick()
│    frame.debug.npcTotal = agentSys.activeAgentCount()
│    frame.debug.fps = 1000.0f / deltaMs
│    ...
│
▼
RenderFrame.debug  (값 타입 전달)
│
▼
SDLRenderer::render()
  └→ drawDebugPanel(frame)  ← ImGui 직접 호출 (Layer 3에만)
```

**레이어 경계 준수:**
- `DebugInfo` 구조체는 `include/core/IRenderCommand.h` (Core API) — SDL2/ImGui 참조 없음 ✅
- ImGui 헤더(`imgui.h`, `imgui_impl_sdl2.h`, `imgui_impl_sdlrenderer2.h`)는 `src/renderer/` 파일에만 존재 ✅
- Domain 시스템(AgentSystem, TransportSystem 등)에 ImGui include 없음 ✅

### 3-2. DebugInfo 구조체

```cpp
// include/core/IRenderCommand.h
struct DebugInfo {
    int     gameTick        = 0;
    int     gameHour        = 0;
    int     gameMinute      = 0;
    int     gameDay         = 1;
    float   simSpeed        = 1.0f;
    bool    isPaused        = false;
    int     npcTotal        = 0;
    int     npcIdle         = 0;
    int     npcWorking      = 0;
    int     npcResting      = 0;
    int     elevatorCount   = 0;
    float   avgSatisfaction = 100.0f;
    float   fps             = 0.0f;
};
```

| 필드 | 출처 (Bootstrapper에서 채움) | 디버그 용도 |
|------|------|------|
| `gameTick` | `SimClock::currentTick()` | 틱 카운터 확인 |
| `gameHour/Minute/Day` | `SimClock::currentTime()` | 인게임 시간 |
| `simSpeed` | Bootstrapper 내 `speed_` 변수 | 배속 확인 |
| `isPaused` | Bootstrapper 내 `paused_` 플래그 | 일시정지 상태 |
| `npcTotal/Idle/Working/Resting` | `AgentSystem::activeAgentCount()` + state 조회 | NPC 분포 |
| `elevatorCount` | `TransportSystem::getAllElevators().size()` | 엘리베이터 수 |
| `avgSatisfaction` | `AgentSystem::getAverageSatisfaction()` | 만족도 |
| `fps` | 프레임 델타타임 역수 | 렌더 성능 |

### 3-3. SDLRenderer ImGui 통합

#### 초기화 순서 (init() 내부)
```
SDL_CreateWindow → SDL_CreateRenderer → ImGui::CreateContext
→ ImGui_ImplSDL2_InitForSDLRenderer → ImGui_ImplSDLRenderer2_Init
```

#### 종료 순서 (shutdown() 내부)
```
ImGui_ImplSDLRenderer2_Shutdown → ImGui_ImplSDL2_Shutdown
→ ImGui::DestroyContext → SDL_DestroyRenderer → SDL_DestroyWindow
```
※ ImGui는 반드시 SDL 파괴 **전에** 종료해야 함

#### 렌더 순서 (render() 내부)
```
1. SDL_RenderClear()
2. drawGridBackground / drawTiles / drawElevators / drawAgents / drawGridLines
3. if (frame.drawDebugInfo):
     ImGui NewFrame → drawDebugPanel() → ImGui Render → RenderDrawData
4. SDL_RenderPresent()
```

### 3-4. InputMapper ImGui 이벤트 처리

```cpp
// processEvent() 최상단
ImGui_ImplSDL2_ProcessEvent(&event);
ImGuiIO& io = ImGui::GetIO();

// ImGui가 키보드/마우스를 점유 중이면 게임 커맨드 생성 안 함
if (io.WantCaptureKeyboard && /* 키보드 이벤트 */) return {};
if (io.WantCaptureMouse && /* 마우스 이벤트 */) return {};
```

이 처리로 ImGui 패널 위에서 마우스 클릭/드래그 시 카메라 이동이 발생하지 않음.

### 3-5. 디버그 패널 레이아웃

```
┌────────────────────────────────┐
│ VSE Debug              [고정]  │
├────────────────────────────────┤
│ Day 1  09:30                   │
│ Tick: 570                      │
│ Speed: 1.0x                    │
├────────────────────────────────┤
│ NPC  total:10  idle:3  work:6  rest:1 │
│ Satisfaction: 100.0%           │
├────────────────────────────────┤
│ FPS: 60.0  Elevators: 2        │
└────────────────────────────────┘
위치: (10, 10) 고정 (ImGuiCond_Always)
크기: 220×180 고정
플래그: NoResize | NoMove | NoCollapse | NoSavedSettings
IniFilename: nullptr (파일 저장 안 함)
```

---

## 4. 테스트 결과

### 4-1. 전체 현황

| 구분 | 테스트 수 | 결과 |
|------|-----------|------|
| 기존 (TASK-01-001~008) | 134 | ✅ 전체 통과 (회귀 없음) |
| TASK-01-009 신규 | 8 | ✅ 전체 통과 |
| **합계** | **142** | **✅ 142/142** |

### 4-2. 신규 테스트 목록 (test_DebugInfo.cpp — 8개)

| # | 테스트명 | 검증 내용 |
|---|---------|-----------|
| 135 | DebugInfo 기본값 검증 | 모든 필드 기본값 (gameTick=0, gameDay=1, simSpeed=1.0 등) |
| 136 | RenderFrame.debug 필드 존재 확인 | frame.debug 접근 + 기본값 |
| 137 | DebugInfo 값 설정/조회 검증 | 전체 필드 설정 후 읽기 정확성 |
| 138 | isPaused 플래그 토글 | false→true→false 토글 |
| 139 | npc 카운트 합산 검증 | idle+working+resting == total 일치 확인 |
| 140 | avgSatisfaction 범위 확인 | 0~100 및 경계값(음수, 120%) 저장 |
| 141 | DebugInfo 복사 및 독립성 | 복사 후 원본 변경 안 됨 확인 |
| 142 | RenderFrame DebugInfo 전체 구조 | RenderFrame 내 모든 debug 필드 통합 검증 |

### 4-3. 빌드 결과

```
[  5%] Built target imgui
[ 11%] Built target spdlog
[ 18%] Built target TowerTycoon
[ 86%] Built target Catch2
[ 87%] Built target Catch2WithMain
[100%] Built target TowerTycoonTests

100% tests passed, 0 tests failed out of 142
Total Test time (real) = 0.37 sec
```

---

## 5. 레이어 경계 검증

```bash
# ImGui include가 renderer/ 에만 존재하는지 확인
grep -rn "imgui" include/ src/domain/ src/core/ → 결과 없음 ✅
grep -rn "imgui" src/renderer/ → SDLRenderer.cpp, InputMapper.cpp 2개 ✅
```

| 파일 | ImGui 포함 여부 | 적합성 |
|------|------|------|
| `include/core/IRenderCommand.h` | ❌ (DebugInfo는 순수 데이터) | ✅ 레이어 준수 |
| `src/core/*.cpp` | ❌ | ✅ |
| `src/domain/*.cpp` | ❌ | ✅ |
| `src/renderer/SDLRenderer.cpp` | ✅ imgui 3개 | ✅ Layer 3 정상 |
| `src/renderer/InputMapper.cpp` | ✅ imgui 3개 | ✅ Layer 3 정상 |

---

## 6. CLAUDE.md 준수 체크리스트

| 규칙 | 상태 | 비고 |
|------|------|------|
| Layer 3에만 ImGui 코드 | ✅ | SDLRenderer.cpp, InputMapper.cpp만 |
| Domain은 ImGui 참조 금지 | ✅ | 전체 확인 완료 |
| DebugInfo는 값 타입 (복사 가능) | ✅ | 순수 POD 구조체 |
| exception 사용 금지 | ✅ | `-fno-exceptions` |
| ImGui IniFilename = nullptr | ✅ | 파일 시스템 오염 방지 |
| ImGui 종료 순서 (SDL 파괴 전) | ✅ | shutdown() 순서 준수 |
| InputMapper WantCapture 처리 | ✅ | ImGui 점유 시 게임 커맨드 차단 |

---

## 7. 미결 사항 / Phase 2 이연

| 항목 | 비고 |
|------|------|
| DebugInfo 실제 값 채우기 (Bootstrapper) | 현재 main.cpp에서 수동 채워야 함 — TASK-01-010(통합)에서 연결 |
| 성능 그래프 (FPS 히스토리) | Phase 2 |
| ImGui 패널 토글 (F3 단축키 연동) | InputMapper에 ToggleDebugOverlay 커맨드 이미 정의됨 — TASK-01-010에서 연결 |
| ImGui 폰트 커스터마이징 | Phase 2 |
| ImGui 창 크기/위치 저장 (ini) | Phase 1에서 의도적 비활성화 |

---

## 8. 교차 검증 요청 항목

1. **DebugInfo 채우기 책임 소재** — Bootstrapper가 매 프레임 채워야 함. TASK-01-010(통합) 시 연결 방식 확인 필요.
2. **FPS 계산 위치** — 현재 main loop에서 직접 계산 후 `frame.debug.fps`에 채우는 방식이 적합한지, 아니면 SDLRenderer 내부에서 계산해야 하는지 설계 결정 필요.
3. **ImGui WantCaptureMouse 이벤트 타입 범위** — 현재 마우스 버튼 + 모션 이벤트만 차단. 마우스 휠(SDL_MOUSEWHEEL)도 차단 여부 확인 필요.

---

## 9. 결론

TASK-01-009는 **Dear ImGui 디버그 패널의 기반 파이프라인을 완성**했다.

- `DebugInfo` 값 타입 구조체로 Domain ↔ Renderer 계약 유지
- ImGui 초기화/종료/렌더링 SDLRenderer에 통합 (Layer 3에만 존재)
- InputMapper WantCapture 처리로 ImGui 패널과 게임 입력 충돌 방지
- 기존 134개 회귀 없음 + 신규 8개 = **142/142 전체 통과**

**DebugInfo 실제 값 채우기는 TASK-01-010(통합) 단계에서 Bootstrapper와 연결 예정.**
