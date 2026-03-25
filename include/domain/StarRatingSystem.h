/**
 * @file StarRatingSystem.h
 * @layer Layer 1 — Domain Module
 * @task TASK-02-004
 * @author DeepSeek V3
 * @reviewed pending
 *
 * @brief StarRatingSystem — computes building star rating (1~5).
 *        Rating is based on NPC average satisfaction and minimum population.
 *        Thresholds are loaded from balance.json "starRating.thresholds".
 *        Emits StarRatingChanged event (deferred) when rating changes.
 *
 * @see VSE_Design_Spec.md §StarRatingComponent
 * @see CLAUDE.md §Layer 0 Module List
 */

#pragma once
#include "core/EventBus.h"
#include "core/IAgentSystem.h"
#include "core/Types.h"
#include <entt/entt.hpp>
#include <array>

namespace vse {

/**
 * @brief StarRatingComponent — singleton component storing current rating stats.
 * 
 * Owned by StarRatingSystem. Created once during initRegistry().
 * Updated every tick in update().
 */
struct StarRatingComponent {
    StarRating currentRating = StarRating::Star1;
    float avgSatisfaction = 50.0f;
    int totalPopulation = 0;
};

/**
 * @brief Payload for StarRatingChanged event.
 */
struct StarRatingChangedPayload {
    StarRating oldRating;
    StarRating newRating;
    float avgSatisfaction;
};

class StarRatingSystem {
public:
    struct Config {
        // satisfaction thresholds for Star1~Star5
        // thresholds[0]=0 (always Star1 floor), thresholds[1]=30, [2]=50, [3]=70, [4]=90
        // from balance.json "starRating.thresholds" (5 values)
        std::array<float, 5> satisfactionThresholds = {0.f, 30.f, 50.f, 70.f, 90.f};
        int minPopulationForStar2 = 1;  // at least 1 NPC to go above Star1
    };

    explicit StarRatingSystem(EventBus& eventBus, const Config& config);

    // Called every tick by Bootstrapper after AgentSystem::update()
    // Updates StarRatingComponent (singleton entity) in registry
    void update(entt::registry& reg, const IAgentSystem& agents, const GameTime& time);

    // Returns current rating (reads from registry StarRatingComponent)
    StarRating getCurrentRating(const entt::registry& reg) const;

    // Returns average satisfaction (reads from registry StarRatingComponent)
    float getAverageSatisfaction(const entt::registry& reg) const;

    // Creates the singleton StarRatingComponent entity if not present
    // Called once during init
    void initRegistry(entt::registry& reg);

private:
    EventBus& eventBus_;
    Config    config_;

    // Compute new rating from satisfaction + population
    StarRating computeRating(float avgSatisfaction, int population) const;
};

} // namespace vse