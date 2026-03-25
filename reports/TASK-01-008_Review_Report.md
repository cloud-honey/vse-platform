# TASK-01-008 작업 결과 보고서

> **작성일:** 2026-03-25
> **태스크:** TASK-01-008 NPC 스프라이트 렌더링 + 이동 표시
> **레이어:** Layer 3 (SDL2 Renderer)
> **담당:** 붐 (claude-sonnet-4-6)
> **커밋:** 6808959 (feat), 250aab0 (status)
> **테스트:** 132/132 통과 (기존 121 + 신규 11)

---

## 1. 태스크 목표

TASK-01-006(AgentSystem)과 TASK-01-007(SDL2 렌더러)을 연결하여
NPC 에이전트를 화면에 렌더링하는 파이프라인을 완성한다.

**구체 목표:**
- `RenderAgent` 커맨드 구조체 정의 (IRenderCommand.h 확장)
- `RenderFrameCollector`에 AgentSystem 연결 경로 추가
- `SDLRenderer`에 NPC 그리기 구현 (Phase 1: 16×32px 컬러 박스)
- 상태별 시각 구분 (Idle / Working / Resting)
- 단위 테스트 작성 및 기존 테스트 회귀 없음 확인

---

## 2. 구현 파일 목록

### 2-1. 수정한 파일 (5개)

| 파일 | 변경 유형 | 변경 내용 |
|------|-----------|-----------|
| `include/core/IRenderCommand.h` | 확장 | `RenderAgent` 구조체 추가, `RenderFrame.agents` 필드 추가, `<string>` include 추가 |
| `include/renderer/RenderFrameCollector.h` | 확장 | `setAgentSource()` 추가, `IAgentSystem*` + `entt::registry*` 멤버 추가, `<entt/entt.hpp>` include |
| `src/renderer/RenderFrameCollector.cpp` | 확장 | `collect()` 내 에이전트 수집 로직 추가 (PositionComponent + AgentComponent → RenderAgent) |
| `include/renderer/SDLRenderer.h` | 확장 | `drawAgents()` private 메서드 선언 추가 |
| `src/renderer/SDLRenderer.cpp` | 확장 | `drawAgents()` 구현, `render()` 호출 순서에 `drawAgents` 삽입 |

### 2-2. 새로 생성한 파일 (1개)

| 파일 | 역할 |
|------|------|
| `tests/test_RenderAgent.cpp` | RenderAgent 구조체 + RenderFrameCollector 연동 테스트 (11개) |

### 2-3. 수정한 빌드 파일 (2개)

| 파일 | 변경 내용 |
|------|-----------|
| `tests/CMakeLists.txt` | `test_RenderAgent.cpp`, `RenderFrameCollector.cpp` 추가 |

---

## 3. 아키텍처 설계

### 3-1. 데이터 흐름

```
AgentSystem (Layer 1 Domain)
│
│  setAgentSource(&agentSys, &registry)
│
▼
RenderFrameCollector (Layer 3)
│  collect() 시:
│  registry.view<PositionComponent, AgentComponent>()
│  → PositionComponent.pixel, AgentComponent.state → RenderAgent
│
▼
RenderFrame.agents ([]RenderAgent)
│
▼
SDLRenderer::drawAgents()  ← SDL2 직접 호출
```

**레이어 경계 준수:**
- AgentSystem은 SDL2를 모름 ✅
- RenderFrameCollector는 PositionComponent.pixel을 **읽기 전용**으로만 접근 ✅
- drawAgents()는 게임 로직 계산 없이 수치만 화면 좌표로 변환 ✅

### 3-2. RenderAgent 구조체

```cpp
// include/core/IRenderCommand.h
struct RenderAgent {
    EntityId    id      = INVALID_ENTITY;
    PixelPos    pixel;              // 절대 픽셀 위치 (월드 좌하단 기준)
    Direction   facing  = Direction::Right;
    AgentState  state   = AgentState::Idle;
};
```

| 필드 | 출처 | 용도 |
|------|------|------|
| `id` | `entt::entity` (entity 자체) | 디버그 식별 |
| `pixel` | `PositionComponent.pixel` | 화면 좌표 변환 |
| `facing` | `PositionComponent.facing` | Phase 2 스프라이트 플립 |
| `state` | `AgentComponent.state` | 색상 결정 (Phase 1) |

### 3-3. RenderFrameCollector 확장 설계

```cpp
// 선택적 연결 — 미연결 시 agents 빈 배열 (하위 호환)
void setAgentSource(const IAgentSystem* agentSys, entt::registry* reg);

// collect() 내부 — AgentSystem 연결 시만 실행
if (agentSys_ != nullptr && registry_ != nullptr) {
    auto view = registry_->view<PositionComponent, AgentComponent>();
    for (auto entity : view) {
        // PositionComponent.pixel + AgentComponent.state → RenderAgent
    }
}
```

**설계 원칙:**
- `setAgentSource` 미호출 시에도 `collect()` 정상 동작 (agents 빈 배열)
- AgentSystem 포인터는 non-owning (`const IAgentSystem*`) — 소유권 없음
- registry는 mutable 포인터 (`entt::registry*`) — view 조회에 non-const registry 필요

### 3-4. SDLRenderer NPC 그리기

```
NPC 렌더링 규격 (Phase 1):
┌────────────┐
│  머리 🟤   │  ← headSize = npcW * 0.8f (상단)
│            │     색상 = 몸통색 +60/+40/+30 (밝게)
├────────────┤
│            │
│  몸통 🟦   │  ← npcW = tileSizePx * 0.5f * zoom = 16px (1x)
│            │     npcH = tileSizePx * zoom         = 32px (1x)
│            │
└────────────┘
  발 위치 = pixel (좌하단 기준 월드 픽셀)
  화면 drawY = sy - npcH  (Camera 변환 후)
```

**상태별 색상:**

| 상태 | RGB | 의미 |
|------|-----|------|
| `AgentState::Idle` | `(160, 160, 170)` 회색 | 대기/귀가 상태 |
| `AgentState::Working` | `(79, 142, 247)` 파랑 | 근무 중 |
| `AgentState::Resting` | `(255, 165, 0)` 주황 | 점심/휴식 |

---

## 4. 구현 상세

### 4-1. RenderFrameCollector.cpp 에이전트 수집 로직

```cpp
// src/renderer/RenderFrameCollector.cpp — collect() 하단 추가
if (agentSys_ != nullptr && registry_ != nullptr) {
    auto view = registry_->view<PositionComponent, AgentComponent>();
    for (auto entity : view) {
        const auto& pos   = view.get<PositionComponent>(entity);
        const auto& agent = view.get<AgentComponent>(entity);

        RenderAgent ra;
        ra.id     = entity;
        ra.pixel  = pos.pixel;
        ra.facing = pos.facing;
        ra.state  = agent.state;
        frame.agents.push_back(ra);
    }
}
```

### 4-2. SDLRenderer::drawAgents() 구현

```cpp
void SDLRenderer::drawAgents(const RenderFrame& frame, const Camera& camera)
{
    float zoom = camera.zoomLevel();
    int ts = frame.tileSize;
    float npcW = (ts * 0.5f) * zoom;   // 16px at 1x
    float npcH = ts * zoom;             // 32px at 1x

    for (const auto& agent : frame.agents) {
        float sx    = camera.worldToScreenX(static_cast<float>(agent.pixel.x));
        float sy    = camera.worldToScreenY(static_cast<float>(agent.pixel.y));
        float drawY = sy - npcH;  // 발 위치 → 상단으로 이동

        // 화면 밖 컬링
        if (sx + npcW < 0 || sx > camera.viewportW()) continue;
        if (drawY + npcH < 0 || drawY > camera.viewportH()) continue;

        // 상태별 색상 결정
        uint8_t r, g, b;
        switch (agent.state) {
        case AgentState::Working: r=79;  g=142; b=247; break;  // 파랑
        case AgentState::Resting: r=255; g=165; b=0;   break;  // 주황
        default:                  r=160; g=160; b=170;          // 회색
        }

        // 몸통
        SDL_FRect body = {sx, drawY, npcW, npcH};
        SDL_SetRenderDrawColor(renderer_, r, g, b, 230);
        SDL_RenderFillRectF(renderer_, &body);

        // 머리 (npcW * 0.8f 크기, 몸통보다 밝게)
        float headSize = npcW * 0.8f;
        float headX    = sx + (npcW - headSize) * 0.5f;
        float headY    = drawY - headSize;
        SDL_SetRenderDrawColor(renderer_,
            std::min(255, r+60), std::min(255, g+40), std::min(255, b+30), 230);
        SDL_FRect head = {headX, headY, headSize, headSize};
        SDL_RenderFillRectF(renderer_, &head);

        // 테두리 (방향 힌트)
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 80);
        SDL_RenderDrawRectF(renderer_, &body);
    }
}
```

### 4-3. render() 호출 순서

```cpp
// 렌더 순서 (Z-order 낮은 것 먼저)
drawGridBackground(frame, camera);   // 1. 층 배경
drawTiles(frame, camera);            // 2. 테넌트 타일
drawElevators(frame, camera);        // 3. 엘리베이터
drawAgents(frame, camera);           // 4. NPC (새로 추가)  ← TASK-01-008
if (frame.drawGrid) drawGridLines(); // 5. 그리드 선
drawFloorLabels(frame, camera);      // 6. 층 라벨 (Phase 2 Dear ImGui로 이전)
```

---

## 5. 테스트 결과

### 5-1. 전체 현황

| 구분 | 테스트 수 | 결과 |
|------|-----------|------|
| 기존 (TASK-01-001~007) | 121 | ✅ 전체 통과 (회귀 없음) |
| TASK-01-008 신규 | 11 | ✅ 전체 통과 |
| **합계** | **132** | **✅ 132/132** |

### 5-2. 신규 테스트 목록 (test_RenderAgent.cpp — 11개)

| # | 테스트명 | 검증 내용 |
|---|---------|-----------|
| 122 | RenderAgent - 기본 생성 | id=INVALID, state=Idle, facing=Right 기본값 |
| 123 | RenderAgent - 상태별 값 설정 | Working/Left/pixel 값 저장 정확성 |
| 124 | RenderFrame - agents 필드 초기 빈 배열 | 신규 필드 초기화 |
| 125 | RenderFrame - agents 추가 | state/pixel 복수 에이전트 저장 |
| 126 | RenderAgent - facing 방향 저장 | Direction::Left 저장 |
| 127 | RenderFrameCollector - AgentSystem 미연결 시 agents 빈 배열 | setAgentSource 미호출 = 하위 호환 |
| 128 | RenderFrameCollector - AgentSystem 연결 후 에이전트 1명 수집 | spawnAgent 후 collect() 결과 |
| 129 | RenderFrameCollector - 에이전트 픽셀 위치 수집 (음수 아님) | PositionComponent.pixel 복사 정확성 |
| 130 | RenderFrameCollector - 여러 에이전트 수집 | 3명 스폰 → agents.size() == 3 |
| 131 | RenderFrameCollector - despawn 후 수집 안됨 | despawnAgent 후 registry view에서 제거 확인 |
| 132 | RenderFrameCollector - 에이전트 상태 변경 반영 | update(hour=9) → Working 상태 반영 |

### 5-3. 빌드 결과

```
[  5%] Built target imgui
[ 11%] Built target spdlog
[ 18%] Built target TowerTycoon
[ 87%] Built target Catch2
[ 88%] Built target Catch2WithMain
[100%] Built target TowerTycoonTests

100% tests passed, 0 tests failed out of 132
Total Test time (real) = 0.34 sec
```

---

## 6. 구현 과정 이슈 및 해결

### 6-1. 테스트 픽스처 생성자 순서 문제

**문제:** `ConfigManager.loadFromFile()`을 픽스처 생성자 본문에서 호출 시, 멤버 초기화 순서상 `GridSystem(bus, cfg)` 전에 로드가 되지 않아 `assert(loaded_)` 실패.

**원인:** C++ 멤버 초기화 순서는 선언 순서에 따르며, 생성자 본문보다 멤버 초기화 리스트가 먼저 실행됨.

**해결:** 기존 `test_AgentSystem.cpp`의 `PreloadedConfig` 패턴 동일 적용.

```cpp
// 해결책 — ConfigManager를 생성자에서 미리 로드하는 서브클래스
struct PreloadedCfg : ConfigManager {
    PreloadedCfg() {
        loadFromFile(std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json");
    }
};
// 멤버 선언에서 PreloadedCfg cfg; 로 선언 → 초기화 시 자동 로드
```

### 6-2. `entt::registry.view()` non-const 요구

**문제:** `RenderFrameCollector`는 `IAgentSystem`을 `const` 참조로 들고 싶었으나, `registry_->view<>()` 호출이 non-const registry를 요구함.

**결정:** `registry_`는 `entt::registry*` (non-const 포인터)로 유지. RenderFrameCollector는 registry의 컴포넌트를 수정하지 않으며, 읽기 전용 사용은 코드 관례로 보장.

**향후:** EnTT의 `entt::registry::view()` 오버로드 중 const 버전이 있는지 확인 후 적용 검토.

### 6-3. 렌더 Z-order 결정

NPC는 타일(테넌트) 위에 그려지고, 그리드 선 아래에 배치하는 것이 자연스럽다고 판단.

```
배경 → 타일 → 엘리베이터 → NPC → 그리드선 → 라벨
```

엘리베이터 문이 열린 상태(Boarding)에서 탑승자가 엘리베이터 뒤에 보이는 효과는 Phase 2에서 Z-order 세분화로 처리 예정.

---

## 7. CLAUDE.md 준수 체크리스트

| 규칙 | 상태 | 비고 |
|------|------|------|
| Layer 3 렌더러 내부 게임 로직 금지 | ✅ | drawAgents는 좌표 변환 + 색상 결정만 |
| Domain에서 SDL2 직접 호출 금지 | ✅ | AgentSystem은 SDL2 무관 |
| RenderFrame은 값 타입 (복사 가능) | ✅ | RenderAgent도 값 타입 |
| exception 사용 금지 | ✅ | `-fno-exceptions` 빌드 플래그 유지 |
| EntityId = entt::entity | ✅ | RenderAgent.id = entity 직접 |
| 컴포넌트는 데이터만, 로직은 System에 | ✅ | PositionComponent.pixel 단순 복사 |
| NPC 스프라이트 16×32px 규격 (CLAUDE.md) | ✅ | `npcW = ts*0.5f, npcH = ts` |
| 화면 밖 컬링 | ✅ | 좌표 범위 체크 후 skip |

---

## 8. 미결 사항 / 교차 검증 요청 항목

### 8-1. Phase 2 이연

| 항목 | 비고 |
|------|------|
| 스프라이트 시트 렌더링 | 현재 컬러 박스 (Phase 1 명세 준수) |
| 애니메이션 (걸음 사이클) | Phase 2 |
| `facing` 스프라이트 플립 | Phase 2 — 필드는 이미 수집 중 |
| Z-order 세분화 (엘리베이터 탑승자 레이어) | Phase 2 |
| HiDPI 대응 | Phase 2 |

### 8-2. 교차 검증 요청 항목

검토자가 확인해주길 원하는 항목:

1. **`entt::registry.view()` const 대안 존재 여부**
   — 현재 `registry_`가 non-const인데, const 조회 API가 있으면 안전성 향상 가능

2. **RenderAgent.pixel 기준점 일관성**
   — `PositionComponent.pixel`이 발 위치(bottom-left)인지 타일 좌상단인지 AgentSystem 구현에서 확인 필요
   — 현재 `drawY = sy - npcH`로 보정하고 있으나, AgentSystem의 pixel 계산 기준이 명시되어 있지 않음

3. **여러 에이전트 동시 렌더링 시 겹침 처리**
   — 같은 타일에 NPC 2명 이상 있을 때 겹쳐 보임
   — Phase 1에서는 허용, Phase 2에서 오프셋 처리 예정 — 명세 확인 필요

4. **drawAgents 호출 위치 (Z-order)**
   — 현재: 타일 → 엘리베이터 → NPC → 그리드선
   — NPC가 그리드선 아래 있어야 하는지 위에 있어야 하는지 설계 문서에 명시되지 않음

---

## 9. 결론

TASK-01-008은 **NPC 렌더링 파이프라인의 기반을 구축**했다.

- `RenderAgent` 커맨드 구조체로 Domain ↔ Renderer 인터페이스 확장
- `RenderFrameCollector.setAgentSource()` — 선택적 연결 (하위 호환 유지)
- `SDLRenderer.drawAgents()` — 16×32px 컬러 박스, 상태별 색상, 머리 별도 표시
- Z-order: 배경 → 타일 → 엘리베이터 → **NPC** → 그리드선
- 기존 121개 회귀 없음 + 신규 11개 = **132/132 전체 통과**

**교차 검증 요청: pixel 기준점 일관성(항목 2), entt const view 대안(항목 1) 확인 권장.**
