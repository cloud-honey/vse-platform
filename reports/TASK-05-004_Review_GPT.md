# TASK-05-004 Review — GPT-5.4
Verdict: Fail

## P0 (Blockers)
- **Speed buttons and toolbar are not wired into gameplay commands.** `SDLRenderer::render()` records clicks via `pendingSpeedChange_` and `pendingBuildAction_`, and accessors exist in `SDLRenderer.h`, but `Bootstrapper::run()` never calls `checkPendingSpeedChange()` or `checkPendingBuildAction()`. Result: the new HUD controls render, but they do not emit `SetSpeed` / build commands into the game loop.
- **Daily-settlement toast subscription is placed in the wrong scope.** In `src/core/Bootstrapper.cpp`, the `EventType::DailySettlement` subscription is nested inside the auto-save `DayChanged` subscription body. That means toast notifications are not registered during normal startup, only become registered when the auto-save branch runs, and may be registered repeatedly on later auto-saves.

## P1 (Should Fix)
- **Test quality does not meet the stated bar.** `tests/test_HUDPanel.cpp` includes multiple placeholder assertions (`REQUIRE(true)`) for the new HUD features, does not use an anonymous namespace for helpers, and does not meaningfully validate toast expiration, speed-button behavior, toolbar behavior, or daily-change formatting.
- **HUD visibility is inconsistent with the new controls.** `HUDPanel::draw()` respects `visible_` / `frame.showHUD`, but `SDLRenderer::render()` still calls `drawSpeedButtons()` and `drawToolbar()` unconditionally. If HUD is hidden, these controls can still appear and remain interactive.

## P2 (Minor)
- Toast queue capacity and duration are implemented as requested (`MAX_TOASTS = 3`, `TOAST_DURATION = 3.0f`), and `updateToasts(dt)` is called from `HUDPanel::draw()`, but expired toasts are erased before decrementing time, so a toast that crosses zero during the current frame is only removed on the next frame.
- `HUDPanel::formatDailyChange()` uses the correct arrow/sign color logic in rendering, but unlike `formatBalance()` it does not guard the `INT64_MIN` absolute-value edge case.

## Summary
The visual HUD additions are mostly present: star icons render with the expected filled/empty Unicode characters, daily-change arrows/colors are logically correct, the toolbar has four buttons, and the ImGui drawing happens inside the `NewFrame()`/`Render()` lifecycle. However, two core features are functionally incomplete: the new HUD controls are not connected to the main loop, and the toast event subscription is scoped incorrectly. Combined with weak placeholder-heavy tests, this should not be accepted as a pass yet.
