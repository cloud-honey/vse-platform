# TASK-03-005 Game HUD — GPT Review
**Reviewer:** GPT-5.4
**Date:** 2026-03-26
**Verdict:** Conditional Pass

## Summary
The HUD panel itself is structurally clean: it stays in Layer 3, uses `RenderFrame` as a render-data contract instead of pulling from domain systems directly, and its ImGui `Begin()` / `End()` usage is balanced in the current control flow. `SDLRenderer` integration is also straightforward and does not break the existing per-frame ImGui lifecycle.

However, I found two important implementation gaps:
1. **`formatBalance()` is not correct for large values** and has an overflow hazard for the minimum signed 64-bit value.
2. **The new HUD data fields are not populated anywhere in the production pipeline** (`RenderFrameCollector` does not fill them, and no other producer in `src/` writes them), so the panel will currently render default values unless some external caller manually injects them.

Because the UI shell is present but correctness/integration are incomplete, this is a **Conditional Pass**, not a full pass.

## Issues Found
| # | Severity | Location | Description | Recommendation |
|---|---|---|---|---|
| 1 | High | `src/renderer/HUDPanel.cpp:52-77` | `formatBalance()` only handles up to one comma group correctly. Example: `1234567890` becomes `₩1234,567890` instead of `₩1,234,567,890`. It also does `balance = -balance` for negatives, which is undefined/overflow for `INT64_MIN`. | Replace with a general thousands-group formatter that works for arbitrary `int64_t` magnitudes. Handle sign without negating `INT64_MIN` directly (e.g. convert via unsigned magnitude logic or string-based formatting). |
| 2 | High | `src/renderer/RenderFrameCollector.cpp` and overall runtime wiring | The HUD fields added to `RenderFrame` (`balance`, `starRating`, `currentTick`, `tenantCount`, `npcCount`, `showHUD`) are not populated anywhere in `src/`. Current grep shows they are only read by `HUDPanel` and defined in `IRenderCommand.h`. That means the HUD will display defaults in real runtime unless another non-reviewed path injects them manually. | Wire these fields in the frame production path, likely where `RenderFrame` is assembled from `SimClock`, economy, star rating, tenant system, and agent counts. If `RenderFrameCollector` owns frame assembly, extend it or add a dedicated HUD data injection step. |
| 3 | Medium | `src/renderer/HUDPanel.cpp:44-47` | `ImGui::Text("T:%d", frame.currentTick);` and `ImGui::Text("T:%d NPC:%d", frame.tenantCount, frame.npcCount);` reuse `T` for both tick and tenant count. This is not a code bug, but it is ambiguous UI text and will make debugging/integration screenshots harder to read. | Use explicit labels such as `Tick:%d` and `Tenants:%d NPC:%d`. |
| 4 | Medium | `src/renderer/HUDPanel.cpp:14-23` | For upcoming click work, the HUD window currently does **not** set `ImGuiWindowFlags_NoInputs` / `NoMouseInputs`. ImGui windows normally capture mouse interaction, so the panel may block world clicks in the top-left area during TASK-03-006. | Decide interaction policy now: if HUD is display-only, add a no-input flag; if clickable later, document z-order/click-routing expectations and test for blocked world input near `(10,10)`. |
| 5 | Low | `include/core/IRenderCommand.h:134-140` | HUD data was added to a Layer 0/Core-facing POD render contract. This is acceptable as render payload, but it does slightly broaden `RenderFrame` beyond pure world-geometry commands into presentation/UI state. | Acceptable for now, but keep `RenderFrame` as passive data only. Do not let Core compute HUD rules or own ImGui-specific semantics. If HUD grows significantly, consider grouping into a nested `RenderHUD`/`HUDInfo` struct for cleanliness. |

## Test Coverage Assessment
Test coverage is **not adequate for the risky parts** of this task.

What is covered:
- `RenderFrame` HUD fields exist and have default values.
- `HUDPanel` visibility toggles work at the data/member level.
- Build/test integration includes the new files.

What is **not actually covered**, despite test names suggesting otherwise:
- `formatBalance()` output strings are not tested at all.
- `formatStars()` output strings are not tested at all.
- Clamping behavior is not tested through the formatter; tests only assign raw values to `RenderFrame`.
- Edge cases requested in review scope (`0`, negative, `MAX_INT64`, `rating=5.0`) are not exercised against formatter output.
- No test validates that `draw()` preserves ImGui state expectations or can be called safely in a frame.
- No test validates that production code populates HUD fields from real systems.

Specific observations:
- `tests/test_HUDPanel.cpp` explicitly says formatter methods are private and therefore not tested. That is the core weakness of the suite.
- Several tests are placeholder/compile-presence tests (`REQUIRE(true)`, `sizeof(frame) > 0`) rather than behavioral tests.
- The current test file would not catch the large-number formatting defect or the missing runtime data wiring.

Minimum additional tests recommended:
1. Formatter unit tests for:
   - `0 -> ₩0`
   - `999 -> ₩999`
   - `1000 -> ₩1,000`
   - `1234567 -> ₩1,234,567`
   - `INT64_MAX` exact grouping
   - negative values including a safe min-edge strategy
2. Star formatter tests for:
   - `0.0 -> ☆☆☆☆☆ (0.0)`
   - `2.5 -> ★★☆☆☆ (2.5)`
   - `5.0 -> ★★★★★ (5.0)`
   - clamp of `-1.0` and `6.0`
3. Runtime integration test that an assembled frame contains non-default HUD values from actual systems.
4. If TASK-03-006 is next, a UI/input test near HUD bounds to verify whether clicks are intentionally captured or passed through.

## Final Verdict
**Conditional Pass**

- **Pass basis:** Layering is mostly acceptable, `HUDPanel` stays in renderer space, and ImGui frame usage is sane.
- **Why not full pass:** there is a real correctness bug in `formatBalance()` and the HUD data path is not fully wired in production code.
- **Blockers for follow-up tasks:**
  - **TASK-03-006 (click interaction):** current HUD may intercept top-left mouse input unless explicitly designed otherwise.
  - **TASK-03-007 (integration):** HUD currently appears at risk of showing default zeros because the new `RenderFrame` fields are not being populated.

Recommended before marking fully done:
1. Fix `formatBalance()`.
2. Populate HUD fields in the real frame assembly path.
3. Replace placeholder tests with formatter + integration tests.
