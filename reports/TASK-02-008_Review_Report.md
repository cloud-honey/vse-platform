# TASK-02-008 Visual Improvements Review Report

**Task:** Visual Improvements (Color Contrast, Lobby, NPC Visibility)  
**Assignee:** 붐2  
**Status:** ✅ Done  
**Date:** 2026-03-25

---

## Summary

Applied visual improvements to the VSE Platform renderer based on Sprint 1 Windows playtest feedback:
buildings blended into background, lobby was hard to distinguish, and NPCs were too small/dark.

Only renderer files were modified. All 213 existing tests pass.

---

## Files Modified

- `src/renderer/SDLRenderer.cpp`
- `src/renderer/RenderFrameCollector.cpp`

---

## Color Changes (Before → After)

### Background (`SDLRenderer.cpp`)

| Element | Before | After |
|---------|--------|-------|
| Background clear color | `(135, 206, 235, 255)` sky blue | `(40, 44, 52, 255)` dark blue-gray (Dracula) |

### Grid Lines (`SDLRenderer.cpp`)

| Element | Before | After |
|---------|--------|-------|
| Grid line color | `(80, 80, 100, 60)` | `(80, 85, 100, 40)` subtle on dark bg |

### Building Tiles (`RenderFrameCollector.cpp`)

| Tenant Type | Before | After |
|-------------|--------|-------|
| Office | `(70, 130, 180, 230)` blue | `(90, 150, 210, 240)` brighter blue |
| Residential | `(60, 179, 113, 230)` green | `(80, 200, 130, 240)` brighter green |
| Commercial | `(255, 165, 0, 230)` orange | `(255, 180, 40, 240)` brighter warm yellow-orange |
| Default/Unknown | `(128, 128, 128, 230)` mid gray | `(160, 160, 170, 230)` lighter gray |

### Lobby & Elevator (`RenderFrameCollector.cpp`)

| Element | Before | After |
|---------|--------|-------|
| Lobby empty tile (floor 0) | `(80, 80, 100, 150)` dark gray | `(70, 75, 90, 200)` higher contrast on dark bg |
| Elevator shaft | `(100, 100, 120, 200)` | `(130, 130, 160, 220)` lighter metallic gray |

### NPC State Colors (`SDLRenderer.cpp`)

| State | Before | After |
|-------|--------|-------|
| Idle | `(160, 160, 170)` gray | `(0, 200, 220)` bright cyan |
| Working | `(79, 142, 247)` blue | `(0, 220, 100)` bright green |
| Resting | `(255, 165, 0)` orange | `(255, 220, 50)` bright yellow |
| WaitingElevator | *(not handled)* | `(180, 100, 255)` bright purple |
| InElevator | *(not handled)* | `(120, 70, 180)` dim purple |

### NPC Outline (`SDLRenderer.cpp`)

| Element | Before | After |
|---------|--------|-------|
| Outline alpha | `60` (nearly invisible) | `180` (visible white outline) |

---

## Test Results

- **Total tests:** 213
- **Passed:** 213
- **Failed:** 0
- **Build:** Clean (warnings pre-existing, not introduced by this task)

---

## Notes

- No domain logic modified
- No interface or test files modified
- Visual changes only — cross-validation not required

---

## Cross-Validation

| Model | Verdict | Key Issues |
|---|---|---|
| Claude (AI) | **Pass** | 이슈 없음. 렌더러 전용 변경, 도메인 영향 없음. P2: Phase 2에서 theme.json 분리 고려 |

*비주얼 변경이라 1모델 리뷰로 충분.*

## GPT-5.4 Post-Review Notes

### P1-1: RenderFrameCollector 역할 명확화
- RenderFrameCollector는 IGridSystem/ITransportSystem에서 데이터를 수집하여 RenderFrame 구조체를 생성하는 Layer 3 유틸리티
- SDLRenderer::render(frame)에 전달하는 구조 — Design Spec 파이프라인과 일치
- Phase 2에서 TileRenderer/AgentRenderer로 분리 시 색상 로직도 이동 예정

### P1-2: "too small" 미해결 명시
- NPC 크기(16x32px)는 변경하지 않음 — 색상 대비 + 아웃라인으로 "dark" 문제만 해결
- 보고서 제목 정정: "Color Contrast, Lobby, NPC Visibility" → 크기 변경 미포함 명시

### P2: cross-validation 문구 수정
- "cross-validation not required" → "레이어 경계 위반 없음, I*.h 인터페이스 변경 없음, 구조 드리프트 없음 확인"

## Cross-Validation (추가)

| Model | Verdict | Key Issues |
|---|---|---|
| Gemini 3 Flash | **Pass** | P2: 텍스트 UI 밝은색 통일, Working 초록색 만족도 혼동 가능성 |
| GPT-5.4 Thinking | **Conditional Pass** | P1: RenderFrameCollector 역할 문서화(→주석 추가), NPC 크기 미변경 보고서 정정(→명시); P2: cross-validation 문구 수정 |
