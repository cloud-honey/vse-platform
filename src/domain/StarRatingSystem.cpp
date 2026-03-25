/**
 * @file StarRatingSystem.cpp
 * @layer Layer 1 — Domain Module
 * @task TASK-02-004
 * @author DeepSeek V3
 * @reviewed pending
 *
 * @brief StarRatingSystem implementation.
 *        Rating formula: if population < minPopulationForStar2 → Star1.
 *        Otherwise: find highest threshold index where avgSatisfaction >= threshold.
 *        StarRatingChanged event emitted (deferred via EventBus) only when rating changes.
 */

#include "domain/StarRatingSystem.h"
#include "core/EventBus.h"
#include "core/IAgentSystem.h"
#include <spdlog/spdlog.h>

namespace vse {

StarRatingSystem::StarRatingSystem(EventBus& eventBus, const Config& config)
    : eventBus_(eventBus)
    , config_(config) {
    // Ensure thresholds are non-decreasing
    for (size_t i = 1; i < config_.satisfactionThresholds.size(); ++i) {
        if (config_.satisfactionThresholds[i] < config_.satisfactionThresholds[i-1]) {
            SPDLOG_ERROR("StarRatingSystem: thresholds must be non-decreasing, but thresholds[{}]={} < thresholds[{}]={}",
                         i, config_.satisfactionThresholds[i], i-1, config_.satisfactionThresholds[i-1]);
            // Fix by making non-decreasing
            config_.satisfactionThresholds[i] = config_.satisfactionThresholds[i-1];
        }
    }
}

void StarRatingSystem::initRegistry(entt::registry& reg) {
    // Create singleton entity if it doesn't exist
    auto view = reg.view<StarRatingComponent>();
    if (view.size() == 0) {
        auto entity = reg.create();
        reg.emplace<StarRatingComponent>(entity);
        SPDLOG_DEBUG("StarRatingSystem: created singleton StarRatingComponent entity {}", 
                     static_cast<uint32_t>(entity));
    } else if (view.size() > 1) {
        SPDLOG_WARN("StarRatingSystem: found {} StarRatingComponent entities, expected 1", view.size());
    }
}

void StarRatingSystem::update(entt::registry& reg, const IAgentSystem& agents, const GameTime& /*time*/) {
    // Get current component
    auto view = reg.view<StarRatingComponent>();
    if (view.size() == 0) {
        SPDLOG_ERROR("StarRatingSystem: no StarRatingComponent found, call initRegistry() first");
        return;
    }
    
    auto entity = *view.begin();
    auto& comp = reg.get<StarRatingComponent>(entity);
    
    // Store old rating for event
    StarRating oldRating = comp.currentRating;
    
    // Get current stats from agents.
    // NOTE: IAgentSystem::getAverageSatisfaction() takes entt::registry& (non-const) for
    //       EnTT view creation, but does not mutate the registry. const_cast is safe here.
    float avgSatisfaction = agents.getAverageSatisfaction(const_cast<entt::registry&>(reg));
    int population = agents.activeAgentCount();
    
    // Compute new rating
    StarRating newRating = computeRating(avgSatisfaction, population);
    
    // Update component
    comp.currentRating = newRating;
    comp.avgSatisfaction = avgSatisfaction;
    comp.totalPopulation = population;
    
    // Emit event if rating changed
    if (oldRating != newRating) {
        StarRatingChangedPayload payload{oldRating, newRating, avgSatisfaction};
        
        eventBus_.publish(Event{
            EventType::StarRatingChanged,
            false,  // replicable
            INVALID_ENTITY,
            payload
        });
        
        SPDLOG_INFO("StarRating changed: {} → {} (satisfaction: {:.1f}, population: {})",
                    static_cast<int>(oldRating), static_cast<int>(newRating),
                    avgSatisfaction, population);
    }
}

StarRating StarRatingSystem::getCurrentRating(const entt::registry& reg) const {
    auto view = reg.view<const StarRatingComponent>();
    if (view.size() == 0) {
        SPDLOG_WARN("StarRatingSystem::getCurrentRating: no StarRatingComponent found");
        return StarRating::Star1;
    }
    
    auto entity = *view.begin();
    return reg.get<const StarRatingComponent>(entity).currentRating;
}

float StarRatingSystem::getAverageSatisfaction(const entt::registry& reg) const {
    auto view = reg.view<const StarRatingComponent>();
    if (view.size() == 0) {
        SPDLOG_WARN("StarRatingSystem::getAverageSatisfaction: no StarRatingComponent found");
        return 50.0f; // Default
    }
    
    auto entity = *view.begin();
    return reg.get<const StarRatingComponent>(entity).avgSatisfaction;
}

StarRating StarRatingSystem::computeRating(float avgSatisfaction, int population) const {
    // If population is below minimum for Star2, always Star1
    if (population < config_.minPopulationForStar2) {
        return StarRating::Star1;
    }
    
    // Find highest threshold that satisfaction meets or exceeds
    // Start from highest threshold (index 4 = Star5) down to index 1 (Star2)
    for (int i = 4; i >= 1; --i) {
        // Use small epsilon for floating point comparison
        if (avgSatisfaction >= config_.satisfactionThresholds[i] - 0.0001f) {
            // i=4 → Star5, i=3 → Star4, i=2 → Star3, i=1 → Star2
            return static_cast<StarRating>(i + 1); // StarRating enum: Star1=1, Star2=2, etc.
        }
    }
    
    // Default: Star1 (satisfaction < threshold[1] = 30)
    return StarRating::Star1;
}

} // namespace vse