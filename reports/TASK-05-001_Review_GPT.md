# TASK-05-001 Review — GPT-5.4
Verdict: Fail

## P0 (Blockers)
- ImGui lifecycle is violated for the tooltip path. `SDLRenderer::render()` calls `buildCursor_.draw(...)` before `ImGui_ImplSDLRenderer2_NewFrame()`, `ImGui_ImplSDL2_NewFrame()`, and `ImGui::NewFrame()` (`src/renderer/SDLRenderer.cpp:144-155`). But `BuildCursor::draw()` may call `drawCostTooltip()`, which uses `ImGui::SetNextWindowPos`, `ImGui::SetNextWindowBgAlpha`, `ImGui::BeginTooltip`, and `ImGui::EndTooltip` (`src/renderer/BuildCursor.cpp:37-39`, `100-118`). ImGui window APIs must run inside an active frame. At best the tooltip will be unreliable; at worst this trips assertions in debug builds. This must be reordered or split into SDL-only draw vs ImGui draw phases.
- Preview validity does not match actual placement behavior at the left edge. In preview calculation, `Bootstrapper` computes `startX = tileX - tenantWidth / 2` and immediately marks placement invalid if any `checkX < 0` (`src/core/Bootstrapper.cpp:344-355`). On click, `InputMapper` computes the same `startX` but clamps it to `0` before emitting `PlaceTenant` (`src/renderer/InputMapper.cpp:145-150`). `BuildCursor::drawTenantHighlight()` also silently skips negative cells instead of clamping (`src/renderer/BuildCursor.cpp:80-96`). Result: the cursor can show an invalid red preview while the click still issues a valid placement command at `x=0`. Preview/command mismatch is a correctness bug.

## P1 (Should Fix)
- Test coverage is materially below the stated requirement and does not exercise the feature behavior. `tests/test_BuildCursor.cpp` contains 5 test cases, not 6+ (`tests/test_BuildCursor.cpp:16-91`). They only verify struct field defaults/copy/assignment; they do not test `isValidPlacement` computation, `previewCost` population, popup open/close flow, out-of-bounds mouse handling, `previewCost == 0` when `BuildAction::None`, or tenant edge-clamping behavior.
- One test is effectively a no-op. `REQUIRE(true)` in `tests/test_BuildCursor.cpp:89-91` does not validate anything meaningful. This should be replaced with assertions against actual behavior.
- `BuildCursor::draw()` returns early for invalid tile coordinates before reaching the tooltip branch (`src/renderer/BuildCursor.cpp:20-23`). Meanwhile `Bootstrapper` explicitly sets `isValidPlacement = false` and `previewCost = 0` for invalid coordinates (`src/core/Bootstrapper.cpp:396-399`). Because of the early return, the user gets no invalid-placement feedback at all when off-grid. If the intended UX is “red highlight or error tooltip on invalid hover,” this implementation drops that state.
- Popup ID handling is fragile. `BuildCursor::drawTenantSelectPopup()` uses a global popup name string (`"Select Tenant Type"`) with no `PushID`/scoping (`src/renderer/BuildCursor.cpp:125-166`). It is fine today with one instance, but it becomes collision-prone if another modal with the same label is introduced or if multiple render/UI components ever reuse that label. A dedicated ID suffix (e.g. `"Select Tenant Type##BuildCursor"`) would make this robust.

## P2 (Minor)
- Color values are still magic numbers in the BuildCursor implementation: valid `(0, 180, 80, 120)`, invalid `(220, 50, 50, 120)`, tooltip error color `(1.0f, 0.3f, 0.3f, 1.0f)` (`src/renderer/BuildCursor.cpp:47-50`, `74-77`, `109`). The inline comments explain intent, so this is not a blocker, but named constants would improve maintainability and consistency.
- Header comments are stale/misleading. `BuildCursor.h` still describes `BuildFloor` as blue highlight and `PlaceTenant` as green highlight (`include/renderer/BuildCursor.h:11-14`), but the implementation now uses green/red validity coloring (`src/renderer/BuildCursor.cpp:46-50`, `73-77`).
- The mock in the test file is unused. `MockRenderer` exists in the anonymous namespace (`tests/test_BuildCursor.cpp:10-13`), so ODR safety is fine, but the mock does not contribute to assertions or coverage.

## Summary
Layering is mostly acceptable: `BuildModeState` remains a plain Layer-0 value type with no logic (`include/core/InputTypes.h:155-166`), and `BuildCursor` does not directly include domain headers (`include/renderer/BuildCursor.h:2-4`, `include/renderer/BuildModeState.h:1-3`). ODR compliance is also fine because the test-only mock is in an anonymous namespace (`tests/test_BuildCursor.cpp:8-13`).

However, I cannot pass this task as implemented. The biggest issue is the ImGui frame-order bug: tooltip rendering is invoked before `ImGui::NewFrame()`. The second major issue is correctness drift between preview validation and actual placement near the left boundary. In addition, the test file does not meet the requested quality bar and leaves the core feature behavior effectively unverified.

Recommended next steps:
1. Move all ImGui calls (`drawCostTooltip`, popup draw) to a post-`ImGui::NewFrame()` phase, or split `BuildCursor` into SDL overlay drawing and ImGui UI drawing functions.
2. Unify tenant placement origin logic across preview, overlay, and click handling. Either clamp in all three places or reject in all three places; they must agree.
3. Replace the placeholder tests with behavior tests and bring the suite to 6+ meaningful cases, including out-of-bounds hover, `BuildAction::None`, invalid tile coordinates, preview cost selection, and popup selection/cancel flow.
