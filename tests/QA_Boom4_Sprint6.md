# Sprint 6 코드 레벨 QA — 붐4 전용

> 작성: 붐 | 2026-03-28 | qwen2.5:72b 실행용
> 목적: macOS ImGui 버튼 문제 원인 파악 + 전체 코드 정합성 검증

## 실행 방법
붐4는 각 섹션을 독립적으로 실행. 결과를 `/home/sykim/.openclaw/workspace/reports/QA_Sprint6_Result.md`에 저장.

---

## SECTION A: ImGui 이벤트 파이프라인 분석 (최우선)

### A-1. 이벤트 흐름 추적
다음 파일들을 읽고 SDL 이벤트가 ImGui에 전달되는 전체 경로를 추적하라:
- `src/core/Bootstrapper.cpp` (run() 함수, 이벤트 루프 부분)
- `src/renderer/InputMapper.cpp` (processEvent 함수)
- `src/renderer/SDLRenderer.cpp` (render 함수, ImGui::NewFrame 전후)

**검증 항목:**
1. `ImGui_ImplSDL2_ProcessEvent`가 호출되는 위치와 시점
2. `ImGui::NewFrame()`이 호출되는 시점
3. 위 두 호출의 순서 — ProcessEvent가 반드시 NewFrame **이전 프레임**에 호출되어야 함
4. 각 프레임에서 이벤트 큐가 비어있을 때 ImGui가 마우스 상태를 어떻게 받는지

**예상 버그 패턴:**
```
Frame N:  SDL_PollEvent → ImGui_ImplSDL2_ProcessEvent (마우스 클릭 전달)
            → processCommands → render → ImGui::NewFrame() → ImGui 버튼 그리기
Frame N+1: SDL_PollEvent → 이미 이벤트 소비됨 → ImGui에 클릭 상태 없음
            → render → ImGui::NewFrame() → 버튼 클릭 감지 안 됨
```

올바른 패턴:
```
Frame N:  ImGui::NewFrame() → ImGui 버튼 그리기 → ImGui::Render()
            SDL_PollEvent → ImGui_ImplSDL2_ProcessEvent (다음 프레임용)
```

**결론 작성:** 현재 코드가 어느 패턴인지, 버그가 있다면 정확한 위치와 수정 방법 명시.

---

### A-2. WantCaptureMouse 분석
`src/renderer/InputMapper.cpp`에서:
1. `io.WantCaptureMouse` 체크 로직 확인
2. MainMenu 상태에서 ImGui 창이 전체 화면을 덮을 때 WantCaptureMouse가 true인지 확인
3. WantCaptureMouse=true일 때 게임 마우스 클릭이 차단되는지 확인
4. ImGui 버튼 클릭이 WantCaptureMouse에 의해 차단되는지 분석

---

### A-3. Paused 상태 분석
`src/renderer/SDLRenderer.cpp`의 drawGameStateUI:
1. Paused 상태에서 ImGui 창 크기/위치 확인
2. Resume 버튼 클릭 → SDL_PushEvent(SDLK_ESCAPE) → InputMapper에서 처리되는지
3. InputMapper에서 SDLK_ESCAPE 처리 로직 확인
4. Paused 상태에서 WantCaptureKeyboard=true로 SDLK_ESCAPE가 차단되는지

---

## SECTION B: 전체 버튼 기능 검증

### B-1. 각 버튼의 전체 처리 경로 추적

다음 파일들에서 각 버튼이 실제로 처리되는 경로를 추적:
- `src/renderer/HUDPanel.cpp`
- `src/renderer/SDLRenderer.cpp` 
- `src/core/Bootstrapper.cpp`
- `src/renderer/InputMapper.cpp`

**버튼별 경로 검증:**

| 버튼 | 발생 위치 | 전달 방법 | 처리 위치 | 올바른가? |
|------|----------|----------|----------|---------|
| Floor | HUDPanel::drawToolbar() → pendingBuildAction_ | Bootstrapper checkPendingBuildAction | inputMapper_.setBuildMode | ? |
| Office | 위와 동일 | 위와 동일 | 위와 동일 | ? |
| Residential | 위와 동일 | 위와 동일 | 위와 동일 | ? |
| Commercial | 위와 동일 | 위와 동일 | 위와 동일 | ? |
| ×1/×2/×3 | HUDPanel::drawSpeedButtons() → pendingSpeedChange_ | Bootstrapper checkPendingSpeedChange | processCommands | ? |
| New Game | SDLRenderer::drawGameStateUI() → SDL_PushEvent(N) | InputMapper SDLK_n | processCommands | ? |
| Load Game | 위와 동일 → SDL_PushEvent(L) | InputMapper SDLK_l | processCommands | ? |
| Quit | 위와 동일 → SDL_PushEvent(Q) | InputMapper SDLK_q | processCommands | ? |
| Resume | 위와 동일 → SDL_PushEvent(ESC) | InputMapper ESC | processCommands | ? |

**각 버튼에 대해 버그 있으면 정확한 코드 위치와 원인 명시.**

---

### B-2. checkPendingBuildAction 타이밍
`src/core/Bootstrapper.cpp`:
1. `sdlRenderer_.checkPendingBuildAction()` 호출 위치 확인
2. 이 호출이 `sdlRenderer_.render()` 이전인지 이후인지 확인
3. render() 안에서 `pendingBuildAction_`이 설정되는데, render 이전에 체크하면 항상 0임 — 이 버그 존재하는가?
4. 동일하게 checkPendingSpeedChange, checkPendingSave, checkPendingLoad 타이밍 확인

---

## SECTION C: 스프라이트 렌더링 검증

### C-1. TileRenderer 로드 경로
`src/renderer/SDLRenderer.cpp`와 `src/renderer/TileRenderer.cpp`:
1. `loadSprites()` 호출 시 spritesDir 경로가 올바른지 (`VSE_PROJECT_ROOT/content/sprites/`)
2. `loadTexture()` 실패 시 nullptr 반환 — drawTile에서 color fallback 동작하는지
3. `drawTile()`의 텍스처 선택 우선순위 로직 정합성:
   - isElevatorShaft → texShaft_ (nullptr이면 color fallback)
   - isFacade → texFacade_ (nullptr이면 color fallback)
   - isLobby → texLobby_
   - tenantType → office/residential/commercial

### C-2. RenderFrameCollector 정합성
`src/renderer/RenderFrameCollector.cpp`:
1. 각 타일 타입에 대해 RenderTile 필드가 올바르게 설정되는지:
   - 엘리베이터 샤프트: isElevatorShaft=true, isFacade=false, isLobby=false
   - 로비 빈 타일(f==0): isLobby=true, isElevatorShaft=false, isFacade=false
   - 상층 빈 타일(f>0, 비샤프트): isFacade=true, isElevatorShaft=false, isLobby=false
   - 점유 타일: tenantType 설정, 나머지 false
2. 플래그 중복/충돌 가능성 있는 케이스 확인

### C-3. NPC 스프라이트 비율
`src/renderer/SpriteSheet.cpp`:
1. frameWidth_ 동적 계산 로직: `frameWidth_ = texW / frameCount_`
2. `npc_sheet.png`가 256×32 (8프레임×32px)이면 frameWidth_=32 — 올바른가?
3. `drawFrame()`에서 srcRect 계산: `{frameIndex * frameWidth_, 0, frameWidth_, frameHeight_}` — 올바른가?

---

## SECTION D: 낮/밤 사이클 검증

### D-1. 오버레이 렌더 순서
`src/renderer/SDLRenderer.cpp`의 render() 함수:
1. 오버레이가 `drawGridBackground` 이후, `drawTiles` 이전에 그려지는지 확인
2. 전체 화면 rect(`nullptr`)에 그려지므로 타일 위에 덮이면 안 됨 — 순서 올바른가?

### D-2. HourChanged 이벤트 연결
`src/renderer/DayNightCycle.cpp`:
1. EventBus 구독이 생성자에서 올바르게 설정되는지
2. `onHourChanged`에서 payload 타입이 int인지 확인 (std::any_cast)
3. SimClock이 HourChanged 이벤트 발행 시 hour 값을 payload에 담는지 (`src/core/SimClock.cpp`)

---

## SECTION E: 세이브/로드 패널 검증

### E-1. SaveLoadPanel 상태 관리
`src/core/Bootstrapper.cpp`:
1. `saveLoadPanelOpen_` 플래그가 render 이전에 frame에 설정되는지
2. `checkPendingSave/checkPendingLoad`가 render 이전에 호출되므로 타이밍 버그 있는지
3. 패널 닫힘 감지 로직 (`wasPanelOpenLastFrame`) 정합성

---

## SECTION F: AudioEngine 안전성

### F-1. VSE_HAS_AUDIO=0 경로
`src/renderer/AudioEngine.cpp`:
1. `#if VSE_HAS_AUDIO == 1` 가드가 모든 메서드에 올바르게 적용되는지
2. VSE_HAS_AUDIO=0일 때 init()이 false 반환하고 크래시 없는지
3. SDLRenderer에서 `audioEngine_.init()` 실패해도 계속 진행되는지

---

## 결과 보고 형식

```
# QA_Sprint6 코드 분석 결과

## 발견된 버그

### [CRITICAL] 버그명
- 파일: 경로
- 라인: 번호
- 원인: 설명
- 수정 방법: 구체적 코드

### [MAJOR] 버그명
...

### [MINOR] 버그명
...

## 정상 항목
- 항목: 확인 완료 이유

## 수정 우선순위
1. (가장 중요한 것부터)
```

결과를 `/home/sykim/.openclaw/workspace/reports/QA_Sprint6_Boom4_Result.md`에 저장.
