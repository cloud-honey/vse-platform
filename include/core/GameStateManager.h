#pragma once
#include "core/Types.h"
#include "core/EventBus.h"
#include <cstdint>

namespace vse {

/**
 * GameStateManager — Simple game state machine.
 *
 * Transitions:
 * - MainMenu → Playing (new game / load game)
 * - Playing → Paused (ESC key)
 * - Playing → GameOver (on GameOver event)
 * - Playing → Victory (on TowerAchieved event)
 * - Paused → Playing (resume)
 * - Paused → MainMenu (quit to menu)
 * - GameOver/Victory → MainMenu (new game)
 *
 * Publishes GameStateChanged event on every transition.
 */
class GameStateManager {
public:
    GameStateManager(EventBus& eventBus);

    // Get current state
    GameState getState() const { return currentState_; }

    // Attempt to transition to new state
    // Returns true if transition is valid and executed
    bool transition(GameState newState);

    // Check if a transition is valid
    bool canTransition(GameState from, GameState to) const;

private:
    EventBus& eventBus_;
    GameState currentState_ = GameState::MainMenu;
};

} // namespace vse