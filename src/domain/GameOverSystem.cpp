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

void GameOverSystem::update(const GameTime& time, int64_t balance, int starRating) {
    if (gameOverFired_ || victoryFired_) {
        return;
    }

    checkBankruptcy(time, balance);
    if (!gameOverFired_) checkMassExodus(time);

    // Victory check only if game over hasn't fired (mutual exclusion)
    if (!gameOverFired_) checkVictory(time, starRating, balance);
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

void GameOverSystem::checkVictory(const GameTime& time, int starRating, int64_t balance) {
    if (victoryFired_) return;

    // Track consecutive positive-balance days (required: 90) — spec §5.21
    if (balance > 0) {
        consecutivePositiveDays_++;
    } else {
        consecutivePositiveDays_ = 0;
    }

    bool starCondition    = (starRating >= 5);
    bool floorCondition   = (grid_.builtFloorCount() >= 100);
    bool npcCondition     = (agents_.activeAgentCount() >= 300);
    bool balanceCondition = (consecutivePositiveDays_ >= 90);

    spdlog::debug("Victory check: star={} floors={} npcs={} positiveDays={}",
                  starRating, grid_.builtFloorCount(), agents_.activeAgentCount(), consecutivePositiveDays_);

    if (starCondition && floorCondition && npcCondition && balanceCondition) {
        victoryFired_ = true;

        TowerAchievedPayload payload;
        payload.day        = time.day;
        payload.starRating = starRating;
        payload.floorCount = grid_.builtFloorCount();
        payload.npcCount   = agents_.activeAgentCount();

        Event ev;
        ev.type    = EventType::TowerAchieved;
        ev.payload = payload;
        eventBus_.publish(ev);
        spdlog::info("VICTORY: TOWER achieved (★{}, {} floors, {} NPCs, {} positive days)",
                     starRating, payload.floorCount, payload.npcCount, consecutivePositiveDays_);
    }
}

void GameOverSystem::checkMassExodus(const GameTime& time) {
    // Mass Exodus: NPC count < 10% of total capacity for 7 consecutive days — spec §5.21
    int capacity = grid_.builtFloorCount() * 10; // 10 NPCs per floor (approx)
    int threshold = capacity / 10;
    int activeNPCs = agents_.activeAgentCount();

    if (capacity > 0 && activeNPCs < threshold) {
        massExodusDays_++;
        spdlog::debug("Mass exodus tracking: day {}, activeNPCs={} < threshold={}, consecutive={}",
                     time.day, activeNPCs, threshold, massExodusDays_);
    } else {
        massExodusDays_ = 0;
    }

    if (massExodusDays_ >= 7 && !gameOverFired_) {
        gameOverFired_ = true;

        GameOverPayload payload;
        payload.reason       = "mass_exodus";
        payload.day          = time.day;
        payload.finalBalance = 0; // Balance not relevant for exodus

        Event ev;
        ev.type    = EventType::GameOver;
        ev.payload = payload;
        eventBus_.publish(ev);
        spdlog::info("GAME OVER: Mass exodus after {} consecutive days (NPCs={} < threshold={})",
                     massExodusDays_, activeNPCs, threshold);
    }
}

void GameOverSystem::reset() {
    consecutiveNegativeDays_ = 0;
    massExodusDays_ = 0;
    consecutivePositiveDays_ = 0;
    gameOverFired_ = false;
    victoryFired_ = false;
    spdlog::debug("GameOverSystem reset");
}

} // namespace vse