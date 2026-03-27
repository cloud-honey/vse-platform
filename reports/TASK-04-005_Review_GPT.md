# TASK-04-005 Review — GPT-5.4

## Problems

### P1 — `New Game` from `GameOver`/`Victory` cannot actually start a new run
- `GameStateManager` only allows `GameOver/Victory -> MainMenu` and rejects direct `-> Playing` transitions (`src/core/GameStateManager.cpp:35-37`).
- But the `NewGame` command path explicitly tries to go straight from `GameOver` or `Victory` to `Playing` (`src/core/Bootstrapper.cpp:424-432`).
- Result: after clicking **New Game** on the Game Over / Victory screens, the world is reset but the state transition fails, so the UI remains stuck in the terminal overlay state instead of entering gameplay.
- This is a user-visible flow break in one of the newly added menu paths, and it is not covered by the new tests.

### P2 — Spec-required menu/state-machine features are still missing from the delivered implementation
- Spec §5.22 requires Main Menu buttons for **New Game / Load Game / Settings / Quit**, plus Save/Load integration with `SaveLoadSystem` (`VSE_Design_Spec.md:1573-1576, 1598-1602`).
- The implemented `GameState` enum has no `Settings` state at all (`include/core/Types.h:123-129`), and the main menu UI exposes only **New Game / Load Game / Quit** (`src/renderer/SDLRenderer.cpp:502-536`).
- `Load Game` and `Save Game` are also placeholders that only change state or log TODOs, without any `SaveLoadSystem` integration (`src/core/Bootstrapper.cpp:437-449`).
- Because §5.22 is the stated target for this task, these omissions leave the feature set materially short of spec.
