/**
 * @file GameStateManager.cpp
 * @layer Core Runtime (src/core/)
 * @task TASK-04-005
 * @author 붐2 (DeepSeek)
 *
 * @brief Game state machine implementation.
 */

#include "core/GameStateManager.h"
#include <spdlog/spdlog.h>

namespace vse {

GameStateManager::GameStateManager(EventBus& eventBus)
    : eventBus_(eventBus) {
    spdlog::debug("GameStateManager initialized with initial state: MainMenu");
}

bool GameStateManager::canTransition(GameState from, GameState to) const {
    // Valid transitions per VSE_Design_Spec.md §5.22:
    // MainMenu → Playing (New Game / Load Game)
    // Playing  → Paused | GameOver | Victory
    // Paused   → Playing (Resume) | MainMenu (Quit to menu)
    // GameOver → MainMenu (reset)
    // Victory  → MainMenu (reset)
    switch (from) {
        case GameState::MainMenu:
            return to == GameState::Playing;

        case GameState::Playing:
            return to == GameState::Paused   ||
                   to == GameState::GameOver ||
                   to == GameState::Victory;

        case GameState::Paused:
            return to == GameState::Playing  ||
                   to == GameState::MainMenu;

        case GameState::GameOver:
        case GameState::Victory:
            // Must go back to MainMenu first before starting a new game
            return to == GameState::MainMenu;

        default:
            return false;
    }
}

bool GameStateManager::transition(GameState newState) {
    GameState oldState = currentState_;
    
    if (!canTransition(oldState, newState)) {
        spdlog::warn("Invalid state transition: {} → {}", 
                     static_cast<int>(oldState), 
                     static_cast<int>(newState));
        return false;
    }
    
    currentState_ = newState;
    spdlog::info("Game state changed: {} → {}", 
                 static_cast<int>(oldState), 
                 static_cast<int>(newState));
    
    // Publish GameStateChanged event
    Event ev;
    ev.type = EventType::GameStateChanged;
    ev.payload = GameStateChangedPayload{oldState, newState};
    eventBus_.publish(ev);
    
    return true;
}

} // namespace vse