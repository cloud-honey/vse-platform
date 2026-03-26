# TASK-04-004 Review — GPT-5.4

## Problems

### P1 — `update()` can emit both `GameOver` and `TowerAchieved` on the same day
- **Where:** `src/domain/GameOverSystem.cpp:24-31`, `src/domain/GameOverSystem.cpp:60-86`
- **Why it matters:** `update()` checks the global end-state guard only once at entry. If the 30th consecutive negative day happens on a frame where the tower conditions are also true, `checkBankruptcy()` sets `gameOverFired_ = true` and publishes `GameOver`, but execution then continues into `checkVictory()`, which does not check `gameOverFired_`. That allows the system to publish `TowerAchieved` as well, creating two mutually exclusive endgame events for one tick/day.
- **Spec impact:** §5.21 / §5.22 model endgame as a single transition (`GameOver` **or** `Victory`), not both.
- **Suggested fix:** Re-check `gameOverFired_` after `checkBankruptcy()` or have `checkVictory()` early-return when `gameOverFired_` is already set.

### P1 — TOWER victory implementation is missing the spec’s 90-day positive-balance requirement
- **Where:** `src/domain/GameOverSystem.cpp:66-71`
- **Spec:** `VSE_Design_Spec.md:1555-1560`
- **Why it matters:** The implementation fires victory when `(star >= 5 && floors >= 100 && npcs >= 300)`, but §5.21 defines TOWER Achievement with an additional requirement: **positive balance for 90 consecutive days**. As written, players can win immediately even if they only just hit the build/star/NPC thresholds, which is materially easier than the specified design.
- **Suggested fix:** Track consecutive positive-balance days alongside bankruptcy tracking and gate `TowerAchieved` on that counter reaching 90.

### P2 — `GameStateChanged` event is declared but not contract-complete or emitted here
- **Where:** `include/core/Types.h:214-217`, repository-wide usage search shows no publisher/consumer in this change
- **Why it matters:** This task introduces `EventType::GameStateChanged`, but `Types.h` does not define a corresponding payload struct, and `GameOverSystem` does not publish the event when transitioning into endgame. If downstream UI/state-machine code is expected to react through `GameStateChanged`, there is no usable event contract yet.
- **Suggested fix:** Either (a) add a concrete `GameStateChangedPayload` and publish it on terminal-state transitions, or (b) remove/defer the enum until the state machine is actually wired.
