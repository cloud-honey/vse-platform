# TASK-04-004 Review Report: Game Over + Victory Conditions

**Reviewer:** DeepSeek R1  
**Date:** 2026-03-27  
**Commit:** 2bc5d16  
**Project:** /home/sykim/.openclaw/workspace/vse-platform/

## Summary
The implementation provides basic game over (bankruptcy) and victory (TOWER) conditions with proper event emission and guard logic. However, there are several **P1** and **P2** issues related to spec compliance, missing features, and potential edge cases.

## Problems Found (P0/P1/P2)

### P1: Missing "Mass Exodus" Game Over Condition
**File:** `include/domain/GameOverSystem.h`, `src/domain/GameOverSystem.cpp`

**Issue:** Design spec §5.21 defines "Mass Exodus" as a game over condition: "NPC count drops below 10% of capacity for 7 days". This is not implemented.

**Impact:** Game lacks a key failure condition, making it easier than intended.

**Fix:** Add mass exodus tracking:
- Track consecutive days with NPC count < 10% of capacity
- Trigger GameOver after 7 consecutive days
- Add `mass_exodus` reason to GameOverPayload

### P1: Missing "Positive Balance for 90 Days" Victory Requirement
**File:** `src/domain/GameOverSystem.cpp` (checkVictory method)

**Issue:** Design spec §5.21 requires "Positive balance for 90 consecutive days" as part of TOWER victory. Implementation only checks star rating, floors, and NPCs.

**Impact:** Victory can be achieved while bankrupt or with negative balance, which contradicts design.

**Fix:** Add positive balance tracking:
- Track consecutive days with balance > 0
- Require 90 consecutive positive days for victory
- Reset counter on negative balance

### P2: Unused `GameStateChanged` Event
**File:** `include/core/Types.h`, `src/domain/GameOverSystem.cpp`

**Issue:** `EventType::GameStateChanged` is defined but never emitted. The spec likely expects state change notifications (e.g., from Playing → GameOver/Victory).

**Impact:** UI systems cannot react to game state transitions via events.

**Fix:** Emit `GameStateChanged` event when:
- GameOver triggers (state → GameOver)
- Victory triggers (state → Victory)
- Game resets (state → Playing)

### P2: Missing `countActiveNPCs()` Implementation
**File:** `include/domain/GameOverSystem.h` (declaration), `src/domain/GameOverSystem.cpp`

**Issue:** Header declares `countActiveNPCs()` method but implementation is missing (empty method body with `(void)reg`). The method uses `agents_.activeAgentCount()` instead.

**Impact:** Dead code that could confuse maintainers. The method should either be implemented or removed.

**Fix:** Either:
1. Implement the method to count NPCs directly from registry (for consistency)
2. Remove the declaration and use `agents_.activeAgentCount()` directly

### P2: No `GameStateChanged` Payload Structure
**File:** `include/core/Types.h`

**Issue:** `EventType::GameStateChanged` exists but no corresponding payload struct is defined.

**Impact:** Cannot pass state transition information (old state, new state, reason).

**Fix:** Add `GameStateChangedPayload` struct:
```cpp
struct GameStateChangedPayload {
    std::string oldState;  // "Playing", "Paused", "GameOver", "Victory"
    std::string newState;
    std::string reason;    // Optional: "bankruptcy", "tower_achieved", etc.
};
```

### P2: Inconsistent Star Rating Type Handling
**File:** `src/domain/GameOverSystem.cpp` (checkVictory), `src/core/Bootstrapper.cpp`

**Issue:** `checkVictory()` takes `int starRating` but `StarRating` is an enum class. Bootstrapper converts enum to int, but this loses type safety.

**Impact:** Potential bugs if wrong integer values are passed.

**Fix:** Change signature to accept `StarRating` enum:
```cpp
void checkVictory(const GameTime& time, const entt::registry& reg, StarRating starRating);
```

### P2: Missing Edge Case: Victory Before Game Over
**File:** `src/domain/GameOverSystem.cpp` (update method)

**Issue:** If both victory and game over conditions are met on the same day, which wins? Current logic checks bankruptcy first, then victory.

**Impact:** Player could achieve victory but get game over instead if bankrupt on same day.

**Fix:** Clarify priority in spec or implement tie-breaking:
- Option 1: Victory takes precedence (more satisfying for player)
- Option 2: Game over takes precedence (more realistic)
- Option 3: Check victory first, then bankruptcy

### P2: No Reset of Positive Balance Counter
**File:** `src/domain/GameOverSystem.cpp` (when P1 fix implemented)

**Issue:** When implementing the "90 consecutive positive days" requirement, need to reset counter on negative balance (similar to bankruptcy counter).

**Impact:** If not implemented, victory could be achieved with intermittent negative days.

**Fix:** Add `consecutivePositiveDays_` counter and reset on negative balance.

### P2: Missing "Critical System Failure" Condition
**File:** Design spec §5.21

**Issue:** Spec mentions "Critical System Failure: Unrecoverable save corruption" but this is likely Phase 2+ feature.

**Impact:** Not critical for Phase 1, but should be noted as deferred.

**Fix:** Add comment/TODO indicating this is deferred to save/load implementation.

### P2: Test Coverage Gaps
**File:** `tests/test_GameOver.cpp`

**Issues:**
1. No test for mass exodus condition (when implemented)
2. No test for positive balance requirement (when implemented)
3. No test for victory/game over tie-breaking
4. No test for GameStateChanged event emission

**Impact:** Reduced confidence in edge case handling.

**Fix:** Add comprehensive tests for all conditions.

## Positive Findings

1. **Bankruptcy logic is correct**: 30 consecutive negative days, counter resets on positive balance.
2. **Guard logic works**: Events fire only once, system stops checking after game end.
3. **Event emission is proper**: GameOver and TowerAchieved events with correct payloads.
4. **Reset functionality works**: Allows new game after game over/victory.
5. **Tests are comprehensive**: 7 tests covering main scenarios, 310/310 passing.
6. **Integration with Bootstrapper**: Properly wired to DayChanged event.

## Recommendations

1. **P1 Issues First**: Implement missing mass exodus and positive balance requirements.
2. **Add GameStateChanged Events**: For UI state machine integration.
3. **Fix Type Safety**: Use `StarRating` enum instead of `int`.
4. **Add Comprehensive Tests**: For all edge cases and new features.
5. **Document Tie-breaking**: Specify victory vs game over priority.

## Overall Assessment

The implementation is **functional but incomplete**. Core bankruptcy and TOWER victory logic works correctly, but missing key spec requirements (mass exodus, positive balance) reduces game balance. Architecture is sound with proper event-driven design. With the P1 fixes, this will be a solid foundation for Phase 1.