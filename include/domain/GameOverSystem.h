/**
 * @file GameOverSystem.h
 * @layer Layer 1 — Domain Module
 * @task TASK-04-004
 * @author Boom2 (DeepSeek)
 *
 * @brief GameOverSystem — handles game over and victory conditions.
 *        Tracks bankruptcy (30 consecutive days of negative balance)
 *        and TOWER victory (★5 + 100 floors + 300 NPCs).
 *
 * @see VSE_Design_Spec.md §5.21
 */

#pragma once
#include "core/IGridSystem.h"
#include "core/IAgentSystem.h"
#include "core/EventBus.h"
#include "core/Types.h"
#include <entt/entt.hpp>

namespace vse {

class GameOverSystem {
public:
    GameOverSystem(IGridSystem& grid, IAgentSystem& agents, EventBus& eventBus);
    
    /**
     * @brief Update game over/victory conditions.
     *        Called on DayChanged event.
     * 
     * @param time Current game time
     * @param reg Entity registry
     * @param balance Current player balance
     * @param starRating Current star rating (0-5)
     */
    void update(const GameTime& time, int64_t balance, int starRating);
    
    /**
     * @brief Check if game over has been triggered.
     */
    bool isGameOver() const { return gameOverFired_; }
    
    /**
     * @brief Check if victory has been triggered.
     */
    bool isVictoryAchieved() const { return victoryFired_; }
    
    /**
     * @brief Reset game state (for new game).
     */
    void reset();
    
private:
    IGridSystem& grid_;
    IAgentSystem& agents_;
    EventBus& eventBus_;
    
    // Bankruptcy tracking
    int consecutiveNegativeDays_ = 0;
    bool gameOverFired_ = false;

    // Mass Exodus tracking
    int massExodusDays_ = 0;            // consecutive days NPC < 10% capacity

    // Victory tracking
    bool victoryFired_ = false;
    int consecutivePositiveDays_ = 0;   // consecutive days with positive balance (90 required)
    
    /**
     * @brief Check bankruptcy condition.
     *        Game over if balance < 0 for 30 consecutive days.
     */
    void checkBankruptcy(const GameTime& time, int64_t balance);
    
    /**
     * @brief Check TOWER victory condition.
     *        Victory requires: ★5 + 100 floors + 300 NPCs + 90 consecutive positive-balance days.
     */
    void checkVictory(const GameTime& time, int starRating, int64_t balance);

    /**
     * @brief Check Mass Exodus game over condition.
     *        Game over if active NPC < 10% of capacity for 7 consecutive days.
     */
    void checkMassExodus(const GameTime& time);
};

} // namespace vse