# TASK-03-006 Build Interaction — Gemini Review
**Reviewer:** Gemini 3.1 Pro
**Date:** 2026-03-26
**Verdict:** Conditional Pass

## Summary
The UI build interaction layer successfully bridges `SDL_Event` logic to Domain commands without violating Layer 3 architectural bounds. The input state machine handles the required key cycles (B, T, ESC) as instructed. However, there are significant coordinate math bugs in the rendering of `BuildCursor`, and a critical discrepancy between the visual preview and the generated placement command that poses a high risk for the upcoming Domain implementation task (TASK-03-007).

## Issues Found
| # | Severity | Location | Description | Recommendation |
|---|---|---|---|---|
| 1 | High | `src/renderer/BuildCursor.cpp` | `drawFloorHighlight` iterates `x` from 0 to `viewportW / tileSize + 2`. When the camera pans right, tile 0 moves off-screen, causing the highlight to render entirely off-screen and become invisible to the user. | Iterate `x` over visible tiles: from `cam.screenToTileX(0)` to `cam.screenToTileX(cam.viewportW())`. |
| 2 | High | `src/renderer/BuildCursor.cpp` | `drawTenantHighlight` creates `SDL_Rect` with width/height set directly to `tileSize`, ignoring `cam.zoomLevel()`. When zoomed out/in, the green preview boxes mismatch the rendered grid tiles. | Multiply the rectangle's width and height by `cam.zoomLevel()`. |
| 3 | Medium | `src/renderer/InputMapper.cpp` | The visual preview centers the tenant (`startX = tileX - width / 2`), but `InputMapper` emits `makePlaceTenant(tileX)` without the centering offset. This will likely cause the Domain to place the building shifted from the preview in TASK-03-007. | Align the logic: either `InputMapper` passes `tileX - width / 2`, or `BuildCursor` starts highlighting exactly at `tileX` (top-left aligned). |
| 4 | Low | `src/renderer/InputMapper.cpp` | Redundant null check. In `SDL_MOUSEBUTTONDOWN`, there is an early `if (camera_ == nullptr) break;`, but inside the `switch` statement, `if (camera_)` is checked again. | Remove the redundant `if (camera_)` checks inside the switch cases. |

## Test Coverage Assessment
- Unit tests (`test_InputMapper.cpp`) adequately cover the `GameCommand` factories and `BuildModeState` round-tripping.
- **Coverage Gap:** `InputMapper::processEvent` is untested. The actual state transitions triggered by `SDL_KEYDOWN` (B, T, ESC) and `SDL_MOUSEBUTTONDOWN` are completely missing from the test suite because `SDL_Event` structs were not synthesized for headless testing.

## Risks for TASK-03-007
The mismatch between `BuildCursor`'s centered visual preview and `InputMapper`'s left-aligned command payload (Issue #3) is the primary risk. If `TenantSystem` assumes the `x` coordinate is the left edge, the constructed building will appear physically offset from where the user clicked/aimed. Both systems must agree on whether the coordinate indicates the center or the left origin.

## Final Verdict
**Conditional Pass**. The architecture matches specifications and bounds are respected. Proceed with merging, provided that the panning offset bug and the zoom scaling bug in `BuildCursor.cpp` are corrected prior to or at the start of TASK-03-007.