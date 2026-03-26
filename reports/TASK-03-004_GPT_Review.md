# TASK-03-004 Camera — GPT Review
**Reviewer:** GPT-5 Mini
**Date:** 2026-03-26
**Verdict:** Conditional Pass

## Summary
The implemented changes add a FontManager wrapper around SDL2_ttf, floor label rendering (drawFloorLabels) with SDL2_ttf fallback, and building exterior visuals (outer frame and zebra floor shading). Code is generally well-structured, tests pass (263/263), and resource cleanup is handled in most places. A few issues and risks were identified that should be addressed before promoting to mainline or before starting TASK-03-005 (ImGui UI).

## Issues Found
| # | Severity | Location | Description | Recommendation |
|---:|:--------:|:--------|:------------|:---------------|
| 1 | Medium | src/renderer/SDLRenderer.cpp (init / shutdown) and src/renderer/FontManager.cpp | SDL_ttf is initialized in two places: SDLRenderer::init calls TTF_Init unconditionally (with warn on failure), and FontManager::load also calls TTF_WasInit() / TTF_Init as a fallback. SDLRenderer::shutdown calls TTF_Quit(). This double-init pattern is functional but fragile: multiple components attempting to own TTF lifecycle can lead to mismatched init/quit ordering if other components load/unload fonts independently. | Centralize TTF lifecycle ownership (prefer SDLRenderer as single owner). Remove TTF_Init from FontManager::load; instead require caller to ensure TTF initialized or make FontManager::load return a clear error if TTF not initialized. If FontManager must try init, use a reference-counted init/quit wrapper. Document ownership. |
| 2 | Medium | src/renderer/FontManager.cpp | FontManager::load calls font_.reset(rawFont) where font_ uses a custom deleter that calls TTF_CloseFont — OK. However FontManager destructor relies solely on unique_ptr; there is no guarding if TTF_Quit is called before fonts are closed (TTF_CloseFont expects valid TTF internals). If TTF_Quit runs while FontManager still exists, behavior is undefined. | Ensure TTF_Quit is called after all FontManager instances are destroyed, or make FontManager explicitly close fonts before TTF_Quit. Prefer centralized lifecycle as above. |
| 3 | Low | src/renderer/SDLRenderer.cpp (drawFloorLabels) | drawFloorLabels computes screenX via camera.worldToScreenX(-10.0f) (a fixed -10 world offset). This anchors labels to a constant world X rather than positioning them relative to building left edge; it may produce wrong horizontal placement when camera is panned/centered or building left edge is elsewhere. Also buildingWidthPx and buildingHeightPx are computed but unused. | Compute screenX relative to buildingLeft (e.g., camera.tileToScreenX(0) - offset) or explicitly document using world X = -10 as intentional. Remove unused buildingWidthPx/Height or use them for alignment. |
| 4 | Low | src/renderer/SDLRenderer.cpp (drawFloorLabels) | Label Y coordinate uses floorWorldY = floor * tileSize; camera.worldToScreenY(floorWorldY + tileSize/2.0f) — this places label at mid-height of tile which is sensible. However floor numbering uses "L{floor}" starting at 0; if design expects L1 for first floor/lobby, this could be inconsistent. | Confirm floor numbering convention with UX spec. If lobby should be L1, adjust label generation accordingly. Add a small comment documenting numbering choice. |
| 5 | Low | src/renderer/SDLRenderer.cpp (drawFloorLabels) | When using SDL_ttf path, code creates SDL_Surface and SDL_Texture per label every frame, then destroys them. This is correct but may be CPU/GPU heavy if labels don't change frequently; repeated per-frame texture creation can be costly. Tests may not cover perf. | Cache rendered label textures per (label string, font size, color) combo (or per-floor) and only recreate on change (font reload, scale change). This will reduce per-frame allocations. If caching complexity is undesired in Phase 1, document perf tradeoff. |
| 6 | Low | src/renderer/SDLRenderer.cpp (drawGridBackground) | Building outer frame is drawn when zoom >= 0.5f — consistent with grid lines and labels. No clipping/SVG-style anti-alias issues obvious. But outerFrame/innerFrame use DrawRectF (stroke) which is fine; ensure stroke width expectations match coordinate transforms on high-DPI displays. | Validate on high-DPI / scaled displays; consider rendering filled rect then inner clears if needed to get consistent visual thickness across zoom/DPI. |

## Test Coverage Assessment
- Tests present: tests/test_FontManager.cpp, tests/test_Camera.cpp, tests/test_RenderFrame.cpp, and many other UI/core tests. The repo reports 263/263 tests passing on commit 66ebb74.
- test_FontManager covers basic load/fallback behavior (file existence and size handling) — good.
- test_Camera and test_RenderFrame likely validate coordinate transforms and rendering decisions — these help ensure label visibility thresholds and camera math are correct.
- Observed gaps: performance characteristics (per-frame SDL_Surface/Texture churn) are not covered by unit tests. High-DPI and resource lifetime edge cases (TTF lifecycle ordering across modules) are not unit-tested.

## Risks for TASK-03-005 (ImGui UI)
- TTF lifecycle: ImGui font handling may also use SDL_ttf or custom font atlas APIs; double-init/quit or ordering issues could conflict when ImGui and FontManager both expect TTF state. Centralizing font subsystem ownership before adding more UI is recommended.
- Texture lifetime: If ImGui or other systems create and hold SDL_Textures (from TTF surfaces) across frames while FontManager or SDLRenderer destroy fonts, ensure textures remain valid — SDL textures are independent of TTF_Font lifetime, but be careful if any code relies on surfaces staying alive. Also, SDL_Renderer must outlive textures; ensure proper shutdown ordering (destroy ImGui resources, textures, then renderer, then TTF_Quit). The current SDLRenderer::shutdown destroys renderer then calls TTF_Quit; it also shuts down ImGui first, which is good, but verify order if other modules hold textures.
- Threading: ImGui/SDL calls must be on main thread; if future UI work introduces background loading of fonts or textures, synchronize with main thread rendering.

## Recommendations (actionable)
1. Centralize SDL_ttf init/quit ownership (prefer SDLRenderer). Remove TTF_Init from FontManager::load or make it safe via a reference-counted init wrapper. Document the ownership clearly.
2. Ensure shutdown ordering: destroy textures and ImGui resources, then destroy renderer/window, then call TTF_Quit. Add comments/tests to validate ordering, or implement an explicit FontSubsystem manager.
3. Replace per-frame TTF surface->texture creation for static labels with a small cache keyed by (label, fontSize, color, scale) to avoid runtime allocation churn.
4. Fix horizontal label placement: use camera.tileToScreenX(0) with negative offset instead of hardcoded world -10 to keep labels visually attached to building regardless of world origin.
5. Confirm floor numbering convention and document (L0 vs L1). Add a unit test asserting label strings to avoid accidental regressions.

## Final Verdict
Conditional Pass — Code is functionally correct, unit tests pass, and changes are reasonably well-contained. Address the medium-severity SDL_ttf lifecycle ownership concerns and the medium/low recommendations above (particularly centralizing TTF init/quit and adjusting label horizontal placement). After those fixes and a quick integration smoke test (high-DPI and ImGui integration), the change is ready to merge and TASK-03-005 may proceed.


---

What I accomplished:
- Reviewed FontManager, Camera, and SDLRenderer changes related to TASK-03-004.
- Verified resource cleanup patterns for TTF_Font and per-frame label surface/texture creation.
- Checked camera math for floor label placement and building exterior drawing.
- Assessed tests present and identified gaps and risks for upcoming ImGui work.

Relevant details for requester:
- Files inspected: include/renderer/FontManager.h, src/renderer/FontManager.cpp, src/renderer/Camera.cpp, src/renderer/SDLRenderer.cpp, plus tests/test_FontManager.cpp and related test files listed in repository.
- Tests pass (repo reported 263/263). See recommendations above for actionable changes.
