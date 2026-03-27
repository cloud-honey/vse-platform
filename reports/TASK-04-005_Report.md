# TASK-04-005 Report: Main Menu + Game State Machine

## Implementation Summary

Implemented game state machine and main menu system as specified in VSE_Design_Spec.md §5.22.

### Changes Made

#### 1. GameState enum in Types.h
- Added `GameState` enum with values: `MainMenu`, `Playing`, `Paused`, `GameOver`, `Victory`
- Added `GameStateChangedPayload` struct for event payload

#### 2. GameStateManager class
- Created `include/core/GameStateManager.h` and `src/core/GameStateManager.cpp`
- Simple state machine with validated transitions:
  - MainMenu → Playing (new game / load game)
  - Playing → Paused (ESC key)
  - Playing → GameOver (on GameOver event)
  - Playing → Victory (on TowerAchieved event)
  - Paused → Playing (resume)
  - Paused → MainMenu (quit to menu)
  - GameOver/Victory → MainMenu (new game)
- Publishes `GameStateChanged` event on every transition
- Methods: `getState()`, `transition()`, `canTransition()`

#### 3. Wired into Bootstrapper
- Added `GameStateManager` member to `Bootstrapper`
- Subscribed to `GameOver` and `TowerAchieved` events to trigger state transitions
- Added `getGameState()` getter
- Modified `TogglePause` command handling to work with game states
- Added game state to `RenderFrame` for UI rendering

#### 4. ImGui menus in SDLRenderer
- Added `drawGameStateUI()` method to render appropriate menus based on game state:
  - **Main Menu**: "New Game", "Load Game", "Quit" buttons
  - **Pause Menu**: "Resume", "Save", "Main Menu", "Quit" buttons (semi-transparent overlay)
  - **Game Over Screen**: Red background with "GAME OVER" and "New Game"/"Quit" buttons
  - **Victory Screen**: Green background with "VICTORY!" and "New Game"/"Quit" buttons
- Buttons generate SDL events that are processed by InputMapper

#### 5. New GameCommand types
- Added to `InputTypes.h`: `NewGame`, `LoadGame`, `SaveGame`, `TransitionState`, `ResetGame`
- Updated `InputMapper` to handle N, L, S, M, Q keys for menu actions
- Updated `Bootstrapper::processCommands()` to handle new commands

#### 6. Tests
- Created `tests/test_GameStateManager.cpp` with 5 tests:
  1. Initial state is MainMenu
  2. Valid transitions work
  3. Invalid transitions are rejected
  4. GameStateChanged event fires on transition
  5. canTransition method works
- Added to CMakeLists.txt

### Technical Details

#### State Transitions
- ESC key now toggles pause (Playing ↔ Paused) instead of quitting
- Quit is now available through menu buttons (Q key or Quit button)
- Game automatically transitions to GameOver/Victory when corresponding events fire

#### UI Implementation
- Used ImGui for menu rendering
- Fullscreen windows with appropriate backgrounds
- Buttons send SDL keyboard events (N, L, S, M, Q) which are mapped to GameCommands
- This avoids tight coupling between UI and game logic

#### Integration Points
- GameStateManager uses EventBus for state change notifications
- Bootstrapper wires GameOverSystem and TowerAchieved events to state transitions
- RenderFrame includes gameState field for UI rendering decisions

### Test Results
- **Total tests**: 315 (310 existing + 5 new)
- **All tests pass**: 100%
- **Build successful**: Yes

### Commit Hash
`a5cdcdc` - feat: TASK-04-005 main menu + game state machine

### Issues / Notes

#### Known Issues
1. **Save/Load functionality**: Save and Load buttons are implemented but call stubs (SaveLoadSystem integration needed)
2. **Game reset**: New Game button in GameOver/Victory screens resets game but could be more thorough
3. **UI polish**: Menu visuals are functional but could use more polish (backgrounds, animations, etc.)
4. **Keyboard shortcuts**: N, L, S, M, Q keys work but might conflict with other game shortcuts

#### Design Decisions
1. **UI Event Handling**: Used SDL event injection from ImGui buttons rather than direct callbacks to maintain Layer 3 separation
2. **State Machine**: Kept simple with explicit transition validation table
3. **Event-driven**: State changes trigger GameStateChanged events for other systems to react
4. **Minimal SDLRenderer changes**: Added single method for UI rendering without major refactoring

#### Future Improvements
1. Integrate with SaveLoadSystem for actual save/load functionality
2. Add settings menu (graphics, audio, controls)
3. Add game statistics/achievements to victory screen
4. Add transition animations between states
5. Add audio feedback for menu interactions

### Verification
- [x] Game starts in Main Menu state
- [x] New Game button starts game in Playing state
- [x] ESC toggles pause (Playing ↔ Paused)
- [x] Pause menu shows correct options
- [x] Game Over event triggers Game Over screen
- [x] Tower Achieved event triggers Victory screen
- [x] All 315 tests pass
- [x] No regression in existing functionality