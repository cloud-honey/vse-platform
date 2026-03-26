/**
 * @file test_GameStateManager.cpp
 * @layer Tests
 * @task TASK-04-005
 * @author 붐2 (DeepSeek)
 *
 * @brief GameStateManager unit tests.
 */

#include <catch2/catch_test_macros.hpp>
#include "core/GameStateManager.h"
#include "core/EventBus.h"
#include <vector>
#include <any>

TEST_CASE("GameStateManager - Initial state is MainMenu", "[GameStateManager]") {
    vse::EventBus eventBus;
    vse::GameStateManager gsm(eventBus);
    
    REQUIRE(gsm.getState() == vse::GameState::MainMenu);
}

TEST_CASE("GameStateManager - Valid transitions work", "[GameStateManager]") {
    vse::EventBus eventBus;
    vse::GameStateManager gsm(eventBus);
    
    // MainMenu → Playing
    REQUIRE(gsm.transition(vse::GameState::Playing) == true);
    REQUIRE(gsm.getState() == vse::GameState::Playing);
    
    // Playing → Paused
    REQUIRE(gsm.transition(vse::GameState::Paused) == true);
    REQUIRE(gsm.getState() == vse::GameState::Paused);
    
    // Paused → Playing
    REQUIRE(gsm.transition(vse::GameState::Playing) == true);
    REQUIRE(gsm.getState() == vse::GameState::Playing);
    
    // Playing → GameOver
    REQUIRE(gsm.transition(vse::GameState::GameOver) == true);
    REQUIRE(gsm.getState() == vse::GameState::GameOver);
    
    // GameOver → MainMenu
    REQUIRE(gsm.transition(vse::GameState::MainMenu) == true);
    REQUIRE(gsm.getState() == vse::GameState::MainMenu);
    
    // MainMenu → Playing → Victory
    REQUIRE(gsm.transition(vse::GameState::Playing) == true);
    REQUIRE(gsm.transition(vse::GameState::Victory) == true);
    REQUIRE(gsm.getState() == vse::GameState::Victory);
    
    // Victory → MainMenu
    REQUIRE(gsm.transition(vse::GameState::MainMenu) == true);
    REQUIRE(gsm.getState() == vse::GameState::MainMenu);
    
    // Playing → Paused → MainMenu
    REQUIRE(gsm.transition(vse::GameState::Playing) == true);
    REQUIRE(gsm.transition(vse::GameState::Paused) == true);
    REQUIRE(gsm.transition(vse::GameState::MainMenu) == true);
    REQUIRE(gsm.getState() == vse::GameState::MainMenu);
}

TEST_CASE("GameStateManager - Invalid transitions are rejected", "[GameStateManager]") {
    vse::EventBus eventBus;
    vse::GameStateManager gsm(eventBus);
    
    // Initial state: MainMenu
    REQUIRE(gsm.getState() == vse::GameState::MainMenu);
    
    // Invalid transitions from MainMenu
    REQUIRE(gsm.transition(vse::GameState::Paused) == false);
    REQUIRE(gsm.transition(vse::GameState::GameOver) == false);
    REQUIRE(gsm.transition(vse::GameState::Victory) == false);
    REQUIRE(gsm.getState() == vse::GameState::MainMenu); // State unchanged
    
    // Transition to Playing
    REQUIRE(gsm.transition(vse::GameState::Playing) == true);
    
    // Invalid transitions from Playing
    REQUIRE(gsm.transition(vse::GameState::MainMenu) == false); // Can't go directly to MainMenu from Playing
    
    // Transition to Paused
    REQUIRE(gsm.transition(vse::GameState::Paused) == true);
    
    // Invalid transitions from Paused
    REQUIRE(gsm.transition(vse::GameState::GameOver) == false);
    REQUIRE(gsm.transition(vse::GameState::Victory) == false);
    
    // Transition to GameOver
    gsm.transition(vse::GameState::Playing); // Back to Playing
    REQUIRE(gsm.transition(vse::GameState::GameOver) == true);
    
    // Invalid transitions from GameOver
    REQUIRE(gsm.transition(vse::GameState::Playing) == false);
    REQUIRE(gsm.transition(vse::GameState::Paused) == false);
    REQUIRE(gsm.transition(vse::GameState::Victory) == false);
}

TEST_CASE("GameStateManager - GameStateChanged event fires on transition", "[GameStateManager]") {
    vse::EventBus eventBus;
    vse::GameStateManager gsm(eventBus);
    
    std::vector<vse::Event> capturedEvents;
    eventBus.subscribe(vse::EventType::GameStateChanged, [&](const vse::Event& e) {
        capturedEvents.push_back(e);
    });
    
    // Perform a transition
    REQUIRE(gsm.transition(vse::GameState::Playing) == true);
    eventBus.flush();
    
    REQUIRE(capturedEvents.size() == 1);
    REQUIRE(capturedEvents[0].type == vse::EventType::GameStateChanged);
    REQUIRE(capturedEvents[0].payload.has_value());
    
    auto* payload = std::any_cast<vse::GameStateChangedPayload>(&capturedEvents[0].payload);
    REQUIRE(payload != nullptr);
    REQUIRE(payload->from == vse::GameState::MainMenu);
    REQUIRE(payload->to == vse::GameState::Playing);
    
    // Another transition
    REQUIRE(gsm.transition(vse::GameState::Paused) == true);
    eventBus.flush();
    
    REQUIRE(capturedEvents.size() == 2);
    REQUIRE(capturedEvents[1].type == vse::EventType::GameStateChanged);
    REQUIRE(capturedEvents[1].payload.has_value());
    
    auto* payload2 = std::any_cast<vse::GameStateChangedPayload>(&capturedEvents[1].payload);
    REQUIRE(payload2 != nullptr);
    REQUIRE(payload2->from == vse::GameState::Playing);
    REQUIRE(payload2->to == vse::GameState::Paused);
}

TEST_CASE("GameStateManager - canTransition method works", "[GameStateManager]") {
    vse::EventBus eventBus;
    vse::GameStateManager gsm(eventBus);
    
    // Valid transitions
    REQUIRE(gsm.canTransition(vse::GameState::MainMenu, vse::GameState::Playing) == true);
    REQUIRE(gsm.canTransition(vse::GameState::Playing, vse::GameState::Paused) == true);
    REQUIRE(gsm.canTransition(vse::GameState::Playing, vse::GameState::GameOver) == true);
    REQUIRE(gsm.canTransition(vse::GameState::Playing, vse::GameState::Victory) == true);
    REQUIRE(gsm.canTransition(vse::GameState::Paused, vse::GameState::Playing) == true);
    REQUIRE(gsm.canTransition(vse::GameState::Paused, vse::GameState::MainMenu) == true);
    REQUIRE(gsm.canTransition(vse::GameState::GameOver, vse::GameState::MainMenu) == true);
    REQUIRE(gsm.canTransition(vse::GameState::Victory, vse::GameState::MainMenu) == true);
    
    // Invalid transitions
    REQUIRE(gsm.canTransition(vse::GameState::MainMenu, vse::GameState::Paused) == false);
    REQUIRE(gsm.canTransition(vse::GameState::MainMenu, vse::GameState::GameOver) == false);
    REQUIRE(gsm.canTransition(vse::GameState::MainMenu, vse::GameState::Victory) == false);
    REQUIRE(gsm.canTransition(vse::GameState::Playing, vse::GameState::MainMenu) == false);
    REQUIRE(gsm.canTransition(vse::GameState::Paused, vse::GameState::GameOver) == false);
    REQUIRE(gsm.canTransition(vse::GameState::Paused, vse::GameState::Victory) == false);
    REQUIRE(gsm.canTransition(vse::GameState::GameOver, vse::GameState::Playing) == false);
    REQUIRE(gsm.canTransition(vse::GameState::GameOver, vse::GameState::Paused) == false);
    REQUIRE(gsm.canTransition(vse::GameState::GameOver, vse::GameState::Victory) == false);
    REQUIRE(gsm.canTransition(vse::GameState::Victory, vse::GameState::Playing) == false);
    REQUIRE(gsm.canTransition(vse::GameState::Victory, vse::GameState::Paused) == false);
    REQUIRE(gsm.canTransition(vse::GameState::Victory, vse::GameState::GameOver) == false);
}