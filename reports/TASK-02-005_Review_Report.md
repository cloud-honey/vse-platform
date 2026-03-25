# VSE Platform — TASK-02-005 Review Report

> Author: claude-sonnet-4-6
> Date: 2026-03-25
> Task: TASK-02-005 NPC Multi-Floor Travel via Elevator
> Layer: Layer 1 — Domain Module
> Tests: 13 new test cases + 174 existing = **187/187 passed**
> Commit: 36d04bb (post-review fix)

---

## Files

| File | Type |
|---|---|
| `include/domain/AgentSystem.h` | Modified — added `transport_` member + new constructor overload |
| `src/domain/AgentSystem.cpp` | Modified — elevator-aware processSchedule, new processElevator |
| `src/domain/TransportSystem.cpp` | Modified — bugfix: Idle state same-floor hall call handling |
| `tests/test_AgentElevator.cpp` | New — 10 elevator integration test cases |
| `tests/CMakeLists.txt` | Modified — added test_AgentElevator.cpp |
| `VSE_Sprint_Status.json` | Modified — TASK-02-005 status → "done" |

---

## Interface

### AgentSystem — new constructor
```cpp
// Original (unchanged, for backward compat)
AgentSystem(IGridSystem& grid, EventBus& bus);

// New overload — elevator support
AgentSystem(IGridSystem& grid, EventBus& bus, ITransportSystem& transport);
```

### AgentSystem — new private member
```cpp
ITransportSystem* transport_;   // nullptr = same-floor only (backward compat)
```

### AgentSystem — new private method
```cpp
void processElevator(entt::registry& reg, EntityId id,
                     AgentComponent& agent,
                     const AgentScheduleComponent& schedule,
                     const GameTime& time);
```

---

## Key Behaviors

### Multi-floor travel flow
1. `processSchedule()` detects state change (Idle→Working or Working→Idle/Resting→Working).
2. If `transport_ != nullptr` and `destFloor != currentFloor`:
   - Sets `agent.state = WaitingElevator`
   - Attaches `ElevatorPassengerComponent{targetFloor, waiting=true}`
   - Calls `transport_->callElevator(0, currentFloor, direction)`
3. `update()` routes `WaitingElevator` and `InElevator` agents to `processElevator()`.
4. `processElevator()`:
   - **Waiting**: polls `getElevatorsAtFloor(currentFloor)`, boards if `state == Boarding`
   - **InElevator**: polls `getAllElevators()`, exits if `currentFloor == targetFloor && (DoorOpening || Boarding)`
   - On exit: updates `PositionComponent.tile.floor`, removes `ElevatorPassengerComponent`, sets state to `Working` (outbound) or `Idle` (return-home)
   - **Phase 1 simplification**: exits directly to `Working`/`Idle` without `Moving` intermediate state. Design Spec specifies `Moving → path to destination` after exit; this is deferred to a later sprint when horizontal pathfinding is integrated.
5. Work-end while elevator:
   - **WaitingElevator (outbound)**: abort → force `Idle`
   - **WaitingElevator (return-home)**: continue waiting
   - **InElevator (outbound)**: re-route `targetFloor` to home floor + issue `callElevator(homeFloor)` hall call so LOOK algorithm stops there
   - **InElevator (return-home)**: continue to home floor, exit → `Idle`

### Backward compatibility
- `AgentSystem(IGridSystem&, EventBus&)` sets `transport_ = nullptr`
- When `transport_ == nullptr`, same-floor-only behavior unchanged (existing tests unaffected)

### TransportSystem bugfix
- Added same-floor hall call handling in `tickCar()` Idle state, matching existing `DoorClosing` pattern
- When elevator already at floor of hall call: immediately transitions to `DoorOpening`
- No existing tests broken (none tested same-floor call from Idle)

---

## Test Cases (13)

| # | Test Name | Result |
|---|---|---|
| 1 | NPC floor 0, workplace floor 2 → WaitingElevator at 9am | ✅ Pass |
| 2 | WaitingElevator NPC boards elevator in Boarding state | ✅ Pass |
| 3 | NPC exits elevator at target floor, PositionComponent.tile.floor updated | ✅ Pass |
| 4 | No ITransportSystem → does NOT enter WaitingElevator (normal Working) | ✅ Pass |
| 5 | ElevatorPassengerComponent attached on WaitingElevator, removed after exit | ✅ Pass |
| 6 | NPC does not double-board while InElevator | ✅ Pass |
| 7 | callElevator called with correct floor when WaitingElevator | ✅ Pass |
| 8 | NPC re-routes to home when workEndHour reached during elevator | ✅ Pass |
| 9 | Two NPCs on floor 0 both board same elevator | ✅ Pass |
| 10 | NPC on same floor as workplace → Working (no WaitingElevator) | ✅ Pass |
| 11 | NPC at floor 2 at workEndHour → WaitingElevator when home is floor 0 | ✅ Pass |
| 12 | NPC re-routes to home floor when workEndHour reached while InElevator | ✅ Pass |
| 13 | NPC arrives at home floor via elevator and transitions to Idle | ✅ Pass |

---

## Deviations from Spec

1. **`InElevator` state used instead of staying `WaitingElevator`**: Spec says set `waiting=false` in `ElevatorPassengerComponent` and keep `WaitingElevator` state while on board. Implementation uses distinct `InElevator` state for clarity — makes `update()` routing and test assertions cleaner. The `InElevator` state was already defined in the `AgentState` enum.

2. **`shaftX = 0` hardcoded**: As specified in Phase 1. Only one shaft is assumed. Phase 2 should use `GridSystem::findNearestShaft()`.

3. **`processSchedule()` refactored**: The original `processSchedule()` only set `agent.state`. Now it also determines `destTenant` to check if floor change is needed. Logic is otherwise equivalent for same-floor cases.

4. **TransportSystem same-floor bugfix**: Not in spec but necessary for correctness. The elevator was stuck `Idle` when the NPC called `callElevator(0, currentFloor, dir)` and the elevator was already at that floor. The `DoorClosing` state already had this pattern — added the same to `Idle`.

5. **AgentSystem polls TransportSystem for boarding/exit decisions (Phase 1 deviation)**: Design Spec designates `TransportSystem::update()` as responsible for passenger boarding, with `ElevatorPassengerComponent` owned by `TransportSystem`. Current implementation has `AgentSystem::processElevator()` poll `getElevatorsAtFloor()` / `getAllElevators()` directly. This simplification is intentional for Phase 1 to avoid bidirectional coupling. Phase 3 refactor target: move boarding/exit logic into `TransportSystem::update()` with event-driven notification to `AgentSystem`.

6. **Exit state is `Working`/`Idle` directly, not `Moving`**: Design Spec elevator flow specifies `Agent exits → AgentState = Moving → path to destination`. Phase 1 omits the post-exit `Moving` step; NPC teleports to floor via `PositionComponent.tile.floor` update. Full horizontal pathfinding integration deferred to a later sprint.

---

## Troubleshooting

### TransportSystem Idle state ignoring same-floor hall calls
- **Symptom**: `callElevator(shaftX=0, floor=0, Up)` with elevator already at floor 0 → elevator stays Idle forever, NPC stuck in WaitingElevator
- **Root cause**: `lookNextTarget()` filters out `f == currentFloor` in the `opposite` bucket and never adds same-floor to `sameDir`. The Idle state path reaches `if (next == -1) break` without opening doors.
- **Fix**: Added same-floor check before `lookNextTarget()` call in Idle state, mirroring existing `DoorClosing` pattern.

### Test tick counting for elevator travel
- Initial tests used fixed tick counts to drive elevator from floor 0 to floor 2. The FSM sequence (DoorOpening → Boarding(3 ticks) → DoorClosing → MovingUp(2 ticks) → DoorOpening) requires ~7 ticks minimum.
- Fixed by using a polling loop (`for 0..15 ticks, check if atTarget`) for robustness against config changes.

---

## Open Items

- **P2**: `shaftX = 0` hardcoded. Phase 2 should resolve nearest shaft from NPC position using `GridSystem.isElevatorShaft()`.
- ~~**P2**: Return-home path (Idle after workEnd) also needs elevator logic when homeTenant is on different floor than current position.~~ **Fixed in Post-Review Fix** (see below).
- **P2**: `lunchHour → Resting` transition doesn't check floor; NPC at workplace may want to go to a restaurant on another floor. Out of scope for Phase 1.
- ~~**P3**: NPC stuck in `InElevator` if elevator breaks (e.g., removed mid-ride). No graceful recovery path yet.~~ Partially addressed: phantom agent issue fixed for workEndHour case.

---

## Cross-Validation

| Model | Verdict | Key Issues |
|---|---|---|
| DeepSeek V3 | Conditional Pass → **Pass (Post-Fix)** | P2: 귀가 경로 미구현(→수정완료), shaftX=0 하드코딩, 점심 층간 이동 미구현 |
| Gemini 3 Flash | Conditional Pass → **Pass (Post-Fix 2)** | P1: 귀가 경로 미완(→수정완료), phantom agent(→수정완료); P2: shaftX=0, waiting bool 중복; P2 re-route hall call 누락 → 실제 버그로 판명(→수정완료, 커밋 4cb28f8) |
| GPT-5.4 Thinking | Conditional Pass → **Pass (Post-Fix 3)** | P1: 보고서 Key Behaviors/Test Cases 수정 전 내용 혼재(→보고서 업데이트완료), 하차 후 Moving 단계 생략(→Phase 1 deviation 명시), FSM 소유권 AgentSystem 폴링 방식(→deviation 명시); P2: 점심 층간 이동 미결(→open item 유지) |

---

## Review Request

Please review this report against `CLAUDE.md` and `VSE_Design_Spec.md`.

**Instructions for the reviewing model:**
1. Use the cross-validation review template below.
2. Include **your model name** in the `Reviewer` field.
3. List **only issues found** — omit anything that passed.
4. Deliver the result as a **Markdown (.md) file**.

**Review template:**

```markdown
# Cross-Validation Review — TASK-02-005

> Reviewer: [YOUR MODEL NAME HERE]
> Verdict: [Pass / Conditional Pass / Fail]

## Issues Found

### P0 — Must Fix Before Next Task
(none, or describe issue)

### P1 — Fix Soon
(none, or describe issue)

### P2 — Nice to Have
(none, or describe issue)

---
*If no issues: write "No issues found." and stop.*
```

---

## Post-Review Fix (2026-03-25)

### Issues Fixed

#### Issue 1: Return-home elevator travel missing (3-model consensus P1)
**Problem**: NPCs used the elevator only for outbound trips (home→work). At `workEndHour`, NPCs on a different floor from home transitioned directly to `Idle` without elevator travel, leaving them stranded on the work floor.

**Root cause**: `processSchedule()` already correctly resolved `homeTenant` destination and called `callElevator()` for the return trip. However, `processElevator()` had an early-return that forced any NPC in `WaitingElevator` or `InElevator` to `Idle` when `time.hour >= workEndHour` — this killed the home-bound elevator trip immediately.

**Fix in `src/domain/AgentSystem.cpp` `processElevator()`**:
- When `workEndHour` is reached and the NPC is in `WaitingElevator`:
  - If `targetFloor == homeFloor` (going home): allow elevator trip to continue (do not abort).
  - If `targetFloor != homeFloor` (going to work): abort and transition to `Idle` (old behavior).
- When `workEndHour` is reached and the NPC is `InElevator` (on board):
  - Do NOT force `Idle` immediately.
  - If elevator is at home floor: exit and transition to `Idle`.
  - Otherwise: re-route `targetFloor` to home floor and let normal exit logic handle arrival.

#### Issue 2: Phantom agent when work ends while InElevator (Gemini P1)
**Problem**: When `workEndHour` was reached while InElevator, the old code forced `Idle` and removed `ElevatorPassengerComponent` — but did NOT call `exitPassenger()` if the elevator was at a non-target floor, leaving a ghost passenger in the TransportSystem.

**Fix in `src/domain/AgentSystem.cpp` `processElevator()`**:
- Changed `targetFloor` to `homeDest.floor` instead of forcing Idle.
- The NPC rides the elevator to the home floor and exits normally via the standard exit logic, which calls `exitPassenger()` correctly.

### Test Changes

| # | Test Name | Change |
|---|---|---|
| 8 | NPC re-routes to home when workEndHour reached during elevator | **Updated** — now verifies re-route behavior instead of forced Idle |
| 11 | NPC at floor 2 at workEndHour → WaitingElevator when home is floor 0 | **New** — verifies return-home elevator call |
| 12 | NPC re-routes to home floor when workEndHour while InElevator | **New** — verifies phantom agent fix |
| 13 | NPC arrives at home floor via elevator and transitions to Idle | **New** — full round-trip test |

### Test Results
- **187/187 tests passed** (184 existing + 3 new)
- No interface headers (I*.h) modified
