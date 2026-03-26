# TASK-03-006 Build Interaction — GPT Review
**Reviewer:** GPT-5.4  
**Date:** 2026-03-26  
**Verdict:** Conditional Pass

## Summary
Implementation is mostly in the intended Layer 3/input-to-command path: `InputMapper` converts SDL input into `GameCommand`, `BuildCursor` only renders visual feedback, and actual world mutation still happens in `Bootstrapper::processCommands()` via `grid_->buildFloor()` / `grid_->placeTenant()` (`src/core/Bootstrapper.cpp:211-252`). That is good and keeps domain writes out of renderer code.

However, I found two material quality issues:
1. **`BuildCursor` geometry is not camera/zoom-correct** for floor-wide highlight rendering, and tenant/floor highlight sizes ignore zoom (`src/renderer/BuildCursor.cpp:34-80`).
2. **Test coverage is much weaker than the feature surface suggests**. The new tests mostly validate structs/factory functions and compile-time presence, not real `InputMapper::processEvent()` behavior (`tests/test_InputMapper.cpp:79-170`).

I also found one architectural smell and one API-safety risk:
- `include/core/IRenderCommand.h` now includes `renderer/BuildModeState.h`, which creates a Core → Renderer dependency inversion.
- `InputMapper::setCamera(const Camera*)` is null-safe at use sites, but it is still a raw non-owning pointer with dangling-pointer risk if reused elsewhere.

## Issues Found
| # | Severity | Location | Description | Recommendation |
|---|---|---|---|---|
| 1 | High | `src/renderer/BuildCursor.cpp:34-52` | `drawFloorHighlight()` uses `cam.viewportW() / tileSize + 2` as the loop bound, so highlight width is based on screen width rather than building/floor width. Under pan/zoom, the highlighted row can under-draw or over-draw relative to the actual building. | Drive floor highlight from authoritative building width (`RenderFrame::floorWidth` or similar), not viewport width. |
| 2 | High | `src/renderer/BuildCursor.cpp:44-48`, `73-77` | Highlight rectangles use raw `tileSize` for width/height, but the rest of renderer uses `tileSize * zoom` (for example `SDLRenderer::drawGridBackground()` / `drawTiles()`). This makes cursor overlays visually incorrect when zoom != 1.0. | Use zoom-scaled rectangle sizes, ideally via camera/world-space conversion rather than raw integer `tileSize`. |
| 3 | Medium | `tests/test_InputMapper.cpp:79-170` | New tests do not exercise actual input mapping behavior. They do not simulate `SDL_KEYDOWN`/`SDL_MOUSEBUTTONDOWN`, so B/ESC/T transitions, click routing, null-camera fallback, and ImGui-capture early returns are effectively untested. | Add event-level tests around `InputMapper::processEvent()` with synthesized `SDL_Event` values. |
| 4 | Medium | `include/core/IRenderCommand.h:2`, `include/core/IRenderCommand.h:141-152` | `IRenderCommand.h` in `core/` now depends on `renderer/BuildModeState.h`. That is not domain-logic leakage, but it is a layer-boundary smell because Core frame data now imports a renderer-defined type. | Move `BuildModeState` to a neutral/shared layer (`core/`, `ui/`, or `presentation/`) so `core` does not include `renderer/...`. |
| 5 | Medium | `include/renderer/InputMapper.h:39-43` | `InputMapper` stores `const Camera* camera_`. Runtime use is null-checked (`src/renderer/InputMapper.cpp:106-129`), so null is handled correctly, but the pointer can still dangle if caller passes a shorter-lived camera object. Current `Bootstrapper` usage is safe because `camera_` is a long-lived member (`src/core/Bootstrapper.cpp:60-67`). | Document lifetime contract clearly, or replace raw pointer with safer API shape (reference injection at call site, setter tied to owner lifetime, or explicit nullable observer wrapper). |
| 6 | Low | `src/renderer/InputMapper.cpp:82-91` | `T` correctly enters `PlaceTenant` and cycles 0→1→2→0 only when already in `PlaceTenant`, but entering `PlaceTenant` from another mode always resets `tenantType` to 0. This may or may not match intended UX. | If the product wants “remember last tenant type,” preserve previous `tenantType` when switching back into `PlaceTenant`; otherwise keep as-is and document it. |

## Test Coverage Assessment
Current coverage is not sufficient for a behavior-heavy input feature.

What is covered:
- `BuildModeState` defaults (`tests/test_InputMapper.cpp:85-91`)
- `setBuildMode()` / `getBuildMode()` value roundtrip (`93-123`)
- `GameCommand::makePlaceTenant()` and `makeBuildFloor()` factories (`125-132`, `166-170`)
- `RenderFrame` field presence (`145-155`)
- A compile-only `setCamera()` smoke check (`157-163`)

What is missing:
- `SDL_KEYDOWN` B → `BuildFloor`
- B again → cancel back to `None`
- `SDL_KEYDOWN` T from `None` → `PlaceTenant` with `tenantType == 0`
- T while already in `PlaceTenant` → exact 0→1→2→0 cycle
- ESC in build mode → cancel without emitting Quit
- ESC outside build mode → emit Quit
- Left click in `BuildFloor` mode → `BuildFloor` command with converted floor
- Left click in `PlaceTenant` mode → `PlaceTenant` command with converted x/floor/type/width
- Left click with null camera → fallback `SelectTile`
- ImGui capture path (`WantCaptureKeyboard` / `WantCaptureMouse`) blocking commands
- `BuildCursor` rendering math under zoom != 1 and camera pan offsets

So although the test count increased, the new tests are mostly structural, not behavioral.

## Final Verdict
**Conditional Pass**

Reasoning:
- **Pass** on the core architectural requirement: domain mutations remain outside renderer/input code and still execute through command handling in `Bootstrapper`.
- **Not a full pass** because the cursor overlay math is not aligned with camera zoom/building width, and the tests do not actually protect the new interaction behavior.

### Risks for TASK-03-007 (integration / Windows build)
1. **Visual mismatch risk during integration:** build cursor overlays will likely look wrong at non-1.0 zoom and may not span the real building width.
2. **Regression risk:** because actual `processEvent()` paths are untested, future fixes for integration could silently break B/T/ESC/click behavior.
3. **Lifetime bug risk:** raw camera pointer is probably safe in current bootstrap flow, but brittle if ownership changes during future refactors.
4. **Windows/MSVC risk:** no obvious platform-specific blocker in this diff, but this feature has not added any Windows-focused coverage. SDL/ImGui event integration and renderer math should be validated on Windows explicitly.

### Recommended gate before TASK-03-007
- Fix `BuildCursor` to use zoom-correct dimensions and real floor/building width.
- Add event-driven `InputMapper` tests for B/T/ESC/click routing.
- Optionally relocate `BuildModeState` out of `renderer/` to remove the Core → Renderer include dependency.
