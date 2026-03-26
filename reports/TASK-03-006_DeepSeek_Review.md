# TASK-03-006 Build Interaction — DeepSeek Review
**Reviewer:** DeepSeek R1
**Date:** 2026-03-26
**Verdict:** Conditional Pass

## Summary
The implementation adds basic building construction interaction with B/T key toggles, mouse‑click handling, and a visual BuildCursor overlay. The architecture respects Layer 3 boundaries (InputMapper → GameCommand → Domain), and the camera pointer is guarded against null. However, several correctness issues were found in coordinate conversion and rendering, and test coverage for the new components is insufficient.

## Issues Found
| # | Severity | Location | Description | Recommendation |
|---|----------|----------|-------------|----------------|
| 1 | High | `InputMapper.cpp` (SDL_MOUSEBUTTONDOWN, default case) | When `buildMode_.action == None`, `makeSelectTile` receives screen‑pixel coordinates (`event.button.x`, `event.button.y`) instead of tile coordinates. The domain will interpret pixels as tile indices, causing incorrect selection. | Convert screen coordinates to tile coordinates using `camera_->screenToTileX` and `camera_->screenToTileFloor`. |
| 2 | High | `BuildCursor.cpp` `drawFloorHighlight` & `drawTenantHighlight` | Both functions use `tileSize` directly as rectangle width/height, ignoring camera zoom. This makes the highlight rectangles mis‑sized when zoomed in/out. Additionally, `drawFloorHighlight` iterates over screen‑column indices (`x < viewportW/tileSize + 2`) instead of world tile indices, causing incorrect highlighting when the camera is panned. | 1. Compute `tileScreenSize = tileSize * cam.zoomLevel()` and use `SDL_FRect` with floating‑point dimensions.<br>2. Determine visible tile range via `startX = cam.screenToTileX(0)`, `endX = cam.screenToTileX(cam.viewportW())` and iterate over world tile indices.<br>3. Use `SDL_RenderFillRectF` for consistent zoomed rendering. |
| 3 | Medium | `BuildCursor.cpp` `drawTenantHighlight` | No upper‑bound check for `currentX`. If the player tries to place a tenant near the right edge of the grid, the highlight may extend beyond the building width. | Clamp `currentX` to `[0, gridWidth‑width]` (where `gridWidth` is available from `RenderFrame`). |
| 4 | Low | `InputMapper.cpp` (T‑key handling) | `tenantWidth` is never updated; it remains fixed at 1. This is fine for Phase 1 but may confuse future extensions. | Consider adding a key (e.g., `[`/`]`) to adjust width, or document that width is fixed for now. |
| 5 | Low | Test suite | No unit tests for `BuildCursor`; the existing `test_InputMapper.cpp` only validates `GameCommand` factories, not mouse‑coordinate conversion or build‑mode transitions. | Add tests for `BuildCursor::draw` with mocked `Camera` and `SDL_Renderer`, and for `InputMapper` mouse‑click conversion in all build modes. |
| 6 | Low | Architecture | `BuildModeState` is placed under `include/renderer/` but also appears in `RenderFrame` (core). This is acceptable because `RenderFrame` is a data carrier between layers, and the state is purely UI‑related. | No change required. |

## Test Coverage Assessment
- **279/279** existing tests pass, confirming no regression.
- **New functionality is under‑tested**:
  - `BuildCursor` has zero unit/integration tests.
  - `InputMapper` mouse‑click conversion (screen → tile) is not validated.
  - Build‑mode state transitions (B/T/ESC) are only covered by compile‑time checks, not runtime behavior.
- **Recommendation**: Add focused tests for coordinate conversion and visual feedback before moving to TASK‑03‑007.

## Windows Build Risks for TASK‑03‑007
The code uses SDL2, which is cross‑platform; no Windows‑specific hazards are introduced. However, the coordinate bugs identified above (issues 1‑2) would affect all platforms equally. Fixing them will also benefit the Windows build.

## Final Verdict
**Conditional Pass** – The implementation is structurally sound and respects the layered architecture, but contains critical correctness bugs in coordinate conversion and rendering that must be fixed before proceeding. The following actions are required:

1. **Immediate fixes** (High severity):
   - Convert screen coordinates to tile coordinates in `InputMapper` for the `SelectTile` command.
   - Correct `BuildCursor` rendering to account for camera zoom and world‑tile indices.
2. **Recommended fixes** (Medium/Low):
   - Add bounds checking for tenant placement.
   - Extend test coverage for new components.

Once these issues are resolved, the feature will be ready for integration and can serve as a solid foundation for TASK‑03‑007.