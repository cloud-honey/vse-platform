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
    void update(const GameTime& time, const entt::registry& reg, int64_t balance, int starRating);
    
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
    
    // Victory tracking
    bool victoryFired_ = false;
    
    /**
     * @brief Check bankruptcy condition.
     *        Game over if balance < 0 for 30 consecutive days.
     */
    void checkBankruptcy(const GameTime& time, int64_t balance);
    
    /**
     * @brief Check TOWER victory condition.
     *        Victory requires:
     *        - Star rating >= 5
     *        - At least 100 floors built
     *        - At least 300 active NPCs
     */
    void checkVictory(const GameTime& time, const entt::registry& reg, int starRating);
    
    /**
     * @brief Count active NPCs in registry.
     */
    int countActiveNPCs(const entt::registry& reg) const;
};

} // namespace vse