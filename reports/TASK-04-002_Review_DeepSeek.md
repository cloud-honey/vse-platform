# TASK-04-002 Review: NPC Stair Movement System

**Reviewer:** DeepSeek R1  
**Date:** 2026-03-26  
**Commit:** ff95f18  
**Verdict:** Conditional Pass (P1 issues must be fixed)

## Summary

The stair movement system has been implemented with core functionality working correctly. The implementation follows the design spec §5.19 for stair preference (≤4 floors), movement speed (2 ticks/floor), and elevator wait timeout fallback. However, there are critical rendering and edge case issues that must be addressed before this can be considered production-ready.

## Issues Found

### P1 (Critical) - Must Fix Before Merge

#### 1. **SDLRenderer Missing UsingStairs State Handling** (P1)
**File:** `src/renderer/SDLRenderer.cpp` (lines 361-373)
**Issue:** The switch statement for AgentState colors does not include `AgentState::UsingStairs`. This means NPCs using stairs will render with the default Idle color (bright cyan) instead of a distinct color.
**Impact:** Visual inconsistency, debugging difficulty, potential confusion for players.
**Fix:** Add a case for `AgentState::UsingStairs` with an appropriate color (e.g., orange for movement).

#### 2. **Missing UsingStairs in SDLRenderer State Checks** (P1)
**File:** `src/renderer/SDLRenderer.cpp`
**Issue:** The renderer's state-based logic may not properly handle UsingStairs for sprite selection or other rendering aspects.
**Impact:** NPCs on stairs may not show walking animations correctly.
**Fix:** Ensure UsingStairs is handled in all renderer state logic, likely falling back to walking animation.

### P2 (Important) - Should Fix

#### 3. **Incomplete Test Coverage for Boundary Cases** (P2)
**Files:** `tests/test_StairMovement.cpp`
**Issue:** Tests don't explicitly verify:
   - Floor difference exactly 4 (should use stairs)
   - Floor difference exactly 5 (should use elevator)
   - Work day ending during stair movement
   - Save/load with NPC in UsingStairs state
**Impact:** Potential undetected edge case bugs.
**Fix:** Add boundary case tests.

#### 4. **Potential State Machine Issue** (P2)
**File:** `src/domain/AgentSystem.cpp` (processStairs method)
**Issue:** When `stairTicksRemaining` reaches 0, the method sets state to Working or Idle based on time, but doesn't check if the NPC actually arrived at the correct destination (workplace vs home).
**Impact:** NPC could arrive at wrong destination if schedule changed during stair movement.
**Fix:** Add destination validation in processStairs completion logic.

### P3 (Minor) - Nice to Have

#### 5. **Hardcoded Tile Size** (P3)
**File:** `src/domain/AgentSystem.cpp` (lines in processStairs and processElevator)
**Issue:** Uses `32.0f` hardcoded instead of config value.
**Impact:** Breaks if tile size changes in config.
**Fix:** Use ConfigManager or constant from defaults.

#### 6. **Magic Numbers in Timeout Logic** (P3)
**File:** `src/domain/AgentSystem.cpp` (line 384)
**Issue:** `> 20` ticks timeout is hardcoded.
**Impact:** Not configurable, hard to balance.
**Fix:** Move to config constants.

## Implementation Quality Assessment

### ✅ Correctly Implemented
1. **UsingStairs AgentState** added to Types.h
2. **Stair decision logic**: ≤4 floors → stairs, >4 floors → elevator
3. **Movement speed**: 2 ticks per floor as specified
4. **Elevator timeout fallback**: >20 ticks wait + ≤4 floors → switch to stairs
5. **Serialization**: stairTargetFloor, stairTicksRemaining, elevatorWaitTicks fields properly serialized
6. **Test updates**: Existing elevator tests updated from floor 0→2 to 0→6 to ensure elevator usage
7. **7 comprehensive tests** covering main scenarios

### ✅ Architecture Compliance
1. **Layer separation maintained**: Logic in domain layer, interfaces in core
2. **Event system used**: AgentStateChanged events fired appropriately
3. **ECS pattern followed**: Components updated correctly
4. **Error handling**: Logging and state recovery for invalid states

### ⚠️ Areas for Improvement
1. **AnimationSystem** handles UsingStairs in default case (walking animation) - acceptable but could be more explicit
2. **Logging**: Good debug logging but some warnings could be more specific
3. **Code duplication**: Some stair/elevator logic duplication that could be refactored

## Recommendations

### Immediate Actions (Before Merge)
1. **Fix SDLRenderer.cpp** to handle UsingStairs state with appropriate color
2. **Add boundary tests** for floor differences 4 and 5
3. **Verify animation system** properly shows walking sprites for UsingStairs

### Short-term Improvements (Next Sprint)
1. **Extract constants** to config (tile size, timeout ticks)
2. **Add more comprehensive tests** for save/load with stair states
3. **Consider refactoring** duplicated floor diff calculation logic

### Long-term Considerations
1. **Stairwell pathfinding** (currently teleports between floors)
2. **Stair congestion** simulation (multiple NPCs using same stairs)
3. **Energy/stamina system** for long stair climbs

## Final Assessment

**Conditional Pass** - The core functionality is correctly implemented and follows the design spec. However, the P1 rendering issue must be fixed before this code can be merged. The P2 issues should be addressed soon after to ensure robustness.

The implementation shows good understanding of the requirements and the VSE platform architecture. With the fixes outlined above, this will be a solid addition to the system.