/**
 * @file GameOverSystem.cpp
 * @layer Layer 1 — Domain Module
 * @task TASK-04-004
 * @author Boom2 (DeepSeek)
 *
 * @brief GameOverSystem implementation.
 */

#include "domain/GameOverSystem.h"
#include "core/EventBus.h"
#include "core/Types.h"
#include "core/IAgentSystem.h"
#include <spdlog/spdlog.h>

namespace vse {

GameOverSystem::GameOverSystem(IGridSystem& grid, IAgentSystem& agents, EventBus& eventBus)
    : grid_(grid)
    , agents_(agents)
    , eventBus_(eventBus) {
}

void GameOverSystem::update(const GameTime& time, const entt::registry& reg, int64_t balance, int starRating) {
    if (gameOverFired_ || victoryFired_) {
        // Game already ended, no further checks
        return;
    }
    
    checkBankruptcy(time, balance);
    checkVictory(time, reg, starRating);
}

void GameOverSystem::checkBankruptcy(const GameTime& time, int64_t balance) {
    if (balance < 0) {
        consecutiveNegativeDays_++;
        spdlog::debug("Bankruptcy tracking: day {}, balance {}, consecutive negative days {}",
                     time.day, balance, consecutiveNegativeDays_);
    } else {
        consecutiveNegativeDays_ = 0;
        spdlog::debug("Bankruptcy tracking: positive balance, reset counter");
    }
    
    if (consecutiveNegativeDays_ >= 30 && !gameOverFired_) {
        gameOverFired_ = true;
        
        GameOverPayload payload;
        payload.reason = "bankruptcy";
        payload.day = time.day;
        payload.finalBalance = balance;
        
        Event ev;
        ev.type = EventType::GameOver;
        ev.payload = payload;
        eventBus_.publish(ev);
        spdlog::info("GAME OVER: Bankruptcy after {} consecutive days of negative balance", consecutiveNegativeDays_);
    }
}

void GameOverSystem::checkVictory(const GameTime& time, const entt::registry& reg, int starRating) {
    (void)reg; // Unused parameter
    if (victoryFired_) {
        return;
    }
    
    // Check all victory conditions
    bool starCondition = (starRating >= 5);
    bool floorCondition = (grid_.builtFloorCount() >= 100);
    bool npcCondition = (agents_.activeAgentCount() >= 300);
    
    if (starCondition && floorCondition && npcCondition) {
        victoryFired_ = true;
        
        TowerAchievedPayload payload;
        payload.day = time.day;
        payload.starRating = starRating;
        payload.floorCount = grid_.builtFloorCount();
        payload.npcCount = agents_.activeAgentCount();
        
        Event ev;
        ev.type = EventType::TowerAchieved;
        ev.payload = payload;
        eventBus_.publish(ev);
        spdlog::info("VICTORY ACHIEVED: TOWER condition met (★{} stars, {} floors, {} NPCs)",
                    starRating, payload.floorCount, payload.npcCount);
    }
}

void GameOverSystem::reset() {
    consecutiveNegativeDays_ = 0;
    gameOverFired_ = false;
    victoryFired_ = false;
    spdlog::debug("GameOverSystem reset");
}

} // namespace vse