# VSE Platform — TASK-02-001 작업 결과 보고서

> 작성일: 2026-03-25
> 검토 대상: Sprint 2 — TASK-02-001 Bootstrapper 클래스 분리 (main.cpp 리팩토링)
> 목적: 외부 AI 교차 검증용

---

## 1. 작업 개요

| 항목 | 내용 |
|------|------|
| 태스크 | TASK-02-001: Bootstrapper 클래스 분리 (main.cpp 리팩토링) |
| 레이어 | Core Runtime (src/core/) |
| 완료일 | 2026-03-25 |
| 커밋 | 2f582a2 (구현) → b6163ad (status 업데이트) → 5c5afcc (대시보드) |
| 테스트 | 신규 5개 + 기존 142개 = **147/147 전부 통과** |

---

## 2. 작업 목표

`src/main.cpp`에 인라인으로 작성된 모든 초기화 코드·게임 루프를 `Bootstrapper` 클래스로 분리.

**Bootstrapper = Composition Root**: 시스템 조립, 초기화 순서 관리, 메인 루프 캡슐화.

변경 전 `main()`:
- 234줄, 시스템 초기화·씬 설정·메인 루프·커맨드 처리·DebugInfo 채우기 전부 인라인

변경 후 `main()`:
```cpp
int main(int /*argc*/, char* /*argv*/[]) {
    vse::Bootstrapper app;
    if (!app.init()) return 1;
    app.run();
    app.shutdown();
    return 0;
}
```

---

## 3. 생성/수정 파일 목록

| 파일 | 변경 유형 | 역할 |
|------|-----------|------|
| `include/core/Bootstrapper.h` | 신규 | Bootstrapper 클래스 선언 |
| `src/core/Bootstrapper.cpp` | 신규 | Bootstrapper 구현 (init/run/shutdown/initDomainOnly) |
| `src/main.cpp` | 수정 | 3줄로 단순화 |
| `tests/test_Bootstrapper.cpp` | 신규 | Bootstrapper 단위 테스트 5개 |
| `tests/CMakeLists.txt` | 수정 | test_Bootstrapper.cpp 추가 |
| `CMakeLists.txt` | 수정 | Bootstrapper.cpp 및 VSE_PROJECT_ROOT 매크로 추가 |
| `src/renderer/SDLRenderer.cpp` | 수정 | shutdown() null guard 추가 |

---

## 4. Bootstrapper 인터페이스

```cpp
class Bootstrapper {
public:
    Bootstrapper() = default;

    // 모든 시스템 초기화 (SDL 포함)
    bool init();

    // 메인 루프 실행
    void run();

    // 정리
    void shutdown();

    // SDL 없이 Domain만 조립 (테스트용)
    bool initDomainOnly(const std::string& configPath = "assets/config/game_config.json");

    // 테스트 전용 접근자
    bool testGetPaused() const;
    int  testGetSpeed() const;
    bool testGetRunning() const;
    void testProcessCommands(const std::vector<GameCommand>& cmds);

private:
    ConfigManager config_;
    EventBus eventBus_;
    entt::registry registry_;
    std::unique_ptr<GridSystem> grid_;
    std::unique_ptr<AgentSystem> agents_;
    std::unique_ptr<TransportSystem> transport_;
    SDLRenderer sdlRenderer_;
    Camera camera_;
    InputMapper inputMapper_;
    std::unique_ptr<RenderFrameCollector> collector_;

    int speed_ = 1;
    bool paused_ = false;
    bool running_ = false;
    SimTick currentTick_ = 0;
    int accumulator_ = 0;

    // 설정 캐시, 렌더 플래그 등...

    void processCommands(const std::vector<GameCommand>& cmds, bool& running);
    void fillDebugInfo(RenderFrame& frame, int realDeltaMs);
};
```

---

## 5. 메인 루프 구조 (CLAUDE.md 준수 확인)

```
run() 내부:
  while (running_) {
    1. handleInput(): SDL_PollEvent → InputMapper → commands
    2. updateSim():
       accumulator_ += realDeltaMs * speed_    ← 배속 구현
       while (accumulator_ >= tickMs_ && ticksThisFrame < MAX_TICKS_PER_FRAME):
         accumulator_ -= tickMs_; ticksThisFrame++
         eventBus_.flush()
         GameTime time = GameTime::fromTick(currentTick_); currentTick_++
         if (ticksThisFrame == 1): processCommands(commands, running_)  ← 첫 tick만
         agents_->update(registry_, time)
         transport_->update(time)
       spiral-of-death 방지: accumulator_ 초과 시 0 리셋
    3. render(): collector_->collect() → sdlRenderer_.render()
  }
```

CLAUDE.md §메인 루프 규칙과 일치 확인:
- [x] `accumulator += realDeltaMs * speed` — 배속 구현
- [x] `MAX_TICKS_PER_FRAME = 8` — spiral-of-death 방지
- [x] `processCommands()` — 첫 tick에서만 drain
- [x] `eventBus_.flush()` — tick 시작 시 호출
- [x] render()는 tick 루프 밖에서 1회

---

## 6. 트러블슈팅 기록

### 문제 1: SDLRenderer 소멸자에서 SIGABRT
- **원인**: `SDLRenderer::~SDLRenderer()`가 `shutdown()`을 호출하는데, 테스트에서 SDL 미초기화 상태에서 ImGui_ImplSDLRenderer2_Shutdown() 호출 → assert 실패
- **수정**: `SDLRenderer::shutdown()`에 `renderer_` null guard 추가
```cpp
void SDLRenderer::shutdown() {
    if (renderer_) {  // ← guard 추가
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow(window_);     window_   = nullptr; }
}
```

### 문제 2: 테스트 접근자가 private에 배치됨
- **원인**: 붐2가 `testGet*()` 메서드를 private 섹션에 넣음
- **수정**: public 섹션으로 이동

### 문제 3: TowerTycoon 타겟에 VSE_PROJECT_ROOT 매크로 미등록
- **원인**: `CMakeLists.txt`에 TowerTycoon 타겟의 `compile_definitions`가 없어서 `Bootstrapper.cpp`의 `initDomainOnly()`에서 경로 해석 실패
- **수정**: `CMakeLists.txt`에 추가
```cmake
target_compile_definitions(TowerTycoon PRIVATE
    VSE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)
```

---

## 7. 설계 정합성 체크리스트

```
[x] Layer 경계 위반 없음 — Bootstrapper는 src/core/ (Core Runtime) 소속
[x] 하드코딩 없음 — 모든 수치 ConfigManager에서 읽음
[x] 네이밍 컨벤션 준수 — camelCase 멤버, PascalCase 클래스
[x] 메모리 소유권 컨벤션 준수 — GridSystem/AgentSystem/TransportSystem/RenderFrameCollector unique_ptr
[x] 에러 핸들링 정책 준수 — exception 없음, init() false 반환 방식
[x] Phase 1 범위 초과 없음 — 기존 기능 이동만, 신규 로직 없음
[x] 단위 테스트 포함 — 5개 (headless, initDomainOnly 기반)
[x] CLAUDE.md와 구현 내용 일치 — 메인 루프 규칙 전부 준수
```

---

## 8. 테스트 목록 (신규 5개)

| # | 테스트명 | 검증 내용 |
|---|----------|-----------|
| 1 | Bootstrapper - Domain 초기화 | initDomainOnly() 성공, running_ == true |
| 2 | Bootstrapper - TogglePause 커맨드 | paused_ false→true→false 토글 |
| 3 | Bootstrapper - SetSpeed 커맨드 | speed_ 1→4 변경 |
| 4 | Bootstrapper - Quit 커맨드 | running_ true→false |
| 5 | Bootstrapper - BuildFloor 커맨드 crash 없음 | processCommands() 정상 처리 |

---

## 9. 미결 검토 항목

1. **testGet*() 공개 여부**: 현재 테스트 전용 public 메서드로 노출. 프로덕션 코드에서 호출 방지 장치 없음. `#ifdef VSE_TESTING` 가드 또는 내부 테스트 전용 빌드 플래그 검토 필요.
2. **processCommands 이중 호출 가능성**: 현재 run()의 커맨드 처리와 tick 루프 내 processCommands가 의도적으로 분리되어 있으나, 주석이 명확하지 않아 혼동 소지 있음.
3. **initDomainOnly() 씬 설정 중복**: init()과 initDomainOnly() 모두 초기 씬(5층/NPC) 설정 코드를 포함해 중복 발생. 별도 `setupInitialScene()` 헬퍼로 추출 검토 필요.

---

## 10. 이번 태스크 교훈 (프로세스)

- **붐2 지시는 영어로 해야 함** — 이번 한국어 지시로 인해 결과물 품질 저하 발생 (마스터 지적으로 확인)
- VSE_AI_Team.md에 이미 명시된 규칙이었으나 붐이 미숙지
- AGENTS.md 절대 규칙에 "비Claude 계열은 영어 지시" 영구 추가 완료
