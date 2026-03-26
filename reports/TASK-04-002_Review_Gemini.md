# TASK-04-002 Review — Gemini 3.1 Pro

## Verdict: Conditional Pass

## Issues

### P1: Elevator wait timeout fallback is dead code
- In `processSchedule`, agents with `floorDiff <= 4` always choose stairs immediately → they NEVER enter `WaitingElevator`
- Agents only enter `WaitingElevator` when `floorDiff >= 5`
- In `processElevator`, the timeout check `if (floorDiff <= 4)` will ALWAYS be false for any agent that naturally entered `WaitingElevator`
- The test passes only because it manually forces an agent into `WaitingElevator` with `floorDiff = 3`, bypassing the natural flow
- **Impact**: The elevator wait timeout → stair fallback feature is completely unreachable in normal gameplay
- **Fix**: Either remove the dead code, or change the fallback to apply regardless of floor difference (e.g., after very long waits, even 5+ floor agents should consider stairs)

### P2: Test coverage gap for edge case
- No test verifies that an agent with `floorDiff = 4` (boundary) correctly chooses stairs
- No test verifies `floorDiff = 5` (boundary) correctly chooses elevator

## Summary
The core stair movement implementation is correct: agents with ≤4 floor difference use stairs at 2 ticks/floor, agents with >4 use elevators, and the state machine transitions are properly handled. However, the elevator wait timeout fallback to stairs is unreachable dead code in normal gameplay because the decision tree prevents agents from ever being in `WaitingElevator` with a small enough floor difference to trigger the fallback. This should be addressed before the feature can be considered complete.
