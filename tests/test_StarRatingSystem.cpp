/**
 * @file test_StarRatingSystem.cpp
 * @layer Layer 1 Test
 * @task TASK-02-004
 * @author DeepSeek V3
 * @reviewed pending
 *
 * @brief Unit tests for StarRatingSystem.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "domain/StarRatingSystem.h"
#include "core/EventBus.h"
#include "core/IAgentSystem.h"
#include <entt/entt.hpp>
#include <memory>

using Catch::Matchers::WithinRel;

namespace vse {

// Mock IAgentSystem for testing
class MockAgentSystem : public IAgentSystem {
public:
    MockAgentSystem() = default;
    
    void setAverageSatisfaction(float value) { avgSatisfaction_ = value; }
    void setActiveAgentCount(int count) { activeCount_ = count; }
    
    // IAgentSystem interface
    Result<EntityId> spawnAgent(entt::registry& reg,
                                EntityId homeTenantId,
                                EntityId workplaceId,
                                std::optional<TileCoord> spawnPos = std::nullopt) override {
        (void)reg;
        (void)homeTenantId;
        (void)workplaceId;
        (void)spawnPos;
        return Result<EntityId>::success(INVALID_ENTITY); // Not used in tests
    }
    
    void despawnAgent(entt::registry& reg, EntityId id) override {
        (void)reg;
        (void)id;
        // Not used in tests
    }
    
    int activeAgentCount() const override {
        return activeCount_;
    }
    
    void update(entt::registry& reg, const GameTime& time) override {
        (void)reg;
        (void)time;
        // Not used in tests
    }
    
    std::optional<AgentState> getState(entt::registry& reg, EntityId id) const override {
        (void)reg;
        (void)id;
        return std::nullopt; // Not used in tests
    }
    
    std::vector<EntityId> getAgentsOnFloor(entt::registry& reg, int floor) const override {
        (void)reg;
        (void)floor;
        return {}; // Not used in tests
    }
    
    std::vector<EntityId> getAgentsInState(entt::registry& reg, AgentState state) const override {
        (void)reg;
        (void)state;
        return {}; // Not used in tests
    }
    
    float getAverageSatisfaction(const entt::registry& reg) const override {
        (void)reg;
        return avgSatisfaction_;
    }
    
private:
    float avgSatisfaction_ = 50.0f;
    int activeCount_ = 0;
};

TEST_CASE("StarRatingSystem basic initialization", "[StarRatingSystem]") {
    EventBus eventBus;
    StarRatingSystem::Config config;
    StarRatingSystem system(eventBus, config);
    
    entt::registry reg;
    
    SECTION("Initial rating is Star1 (empty registry, no NPCs)") {
        system.initRegistry(reg);
        auto rating = system.getCurrentRating(reg);
        REQUIRE(rating == StarRating::Star1);
    }
    
    SECTION("initRegistry() creates exactly one StarRatingComponent entity") {
        system.initRegistry(reg);
        auto view = reg.view<StarRatingComponent>();
        REQUIRE(view.size() == 1);
        
        // Calling again should not create another
        system.initRegistry(reg);
        REQUIRE(view.size() == 1);
    }
}

TEST_CASE("StarRatingSystem rating computation", "[StarRatingSystem]") {
    EventBus eventBus;
    StarRatingSystem::Config config;
    config.satisfactionThresholds = {0.f, 30.f, 50.f, 70.f, 90.f};
    config.minPopulationForStar2 = 1;
    StarRatingSystem system(eventBus, config);
    
    entt::registry reg;
    system.initRegistry(reg);
    
    MockAgentSystem mockAgents;
    
    SECTION("Rating stays Star1 when population == 0") {
        mockAgents.setActiveAgentCount(0);
        mockAgents.setAverageSatisfaction(100.0f); // High satisfaction but no population
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star1);
    }
    
    SECTION("Rating becomes Star2 when satisfaction >= 30 and population >= 1") {
        mockAgents.setActiveAgentCount(1);
        mockAgents.setAverageSatisfaction(30.0f);
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star2);
    }
    
    SECTION("Rating becomes Star3 when satisfaction >= 50") {
        mockAgents.setActiveAgentCount(1);
        mockAgents.setAverageSatisfaction(50.0f);
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star3);
    }
    
    SECTION("Rating becomes Star4 when satisfaction >= 70") {
        mockAgents.setActiveAgentCount(1);
        mockAgents.setAverageSatisfaction(70.0f);
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star4);
    }
    
    SECTION("Rating becomes Star5 when satisfaction >= 90") {
        mockAgents.setActiveAgentCount(1);
        mockAgents.setAverageSatisfaction(90.0f);
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star5);
    }
    
    SECTION("Rating drops back when satisfaction falls below threshold") {
        mockAgents.setActiveAgentCount(1);
        
        // Start at Star5
        mockAgents.setAverageSatisfaction(95.0f);
        GameTime time1{0, 0, 0};
        system.update(reg, mockAgents, time1);
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star5);
        
        // Drop to Star3
        mockAgents.setAverageSatisfaction(60.0f);
        GameTime time2{0, 0, 1};
        system.update(reg, mockAgents, time2);
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star3);
        
        // Drop to Star1
        mockAgents.setAverageSatisfaction(20.0f);
        GameTime time3{0, 0, 2};
        system.update(reg, mockAgents, time3);
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star1);
    }
    
    SECTION("getCurrentRating() returns correct value after update") {
        mockAgents.setActiveAgentCount(5);
        mockAgents.setAverageSatisfaction(75.0f);
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star4);
    }
    
    SECTION("getAverageSatisfaction() returns correct value after update") {
        mockAgents.setActiveAgentCount(3);
        mockAgents.setAverageSatisfaction(65.0f);
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE_THAT(system.getAverageSatisfaction(reg), WithinRel(65.0f, 0.01f));
    }
}

TEST_CASE("StarRatingSystem event emission", "[StarRatingSystem]") {
    EventBus eventBus;
    StarRatingSystem::Config config;
    config.satisfactionThresholds = {0.f, 30.f, 50.f, 70.f, 90.f};
    config.minPopulationForStar2 = 1;
    StarRatingSystem system(eventBus, config);
    
    entt::registry reg;
    system.initRegistry(reg);
    
    MockAgentSystem mockAgents;
    mockAgents.setActiveAgentCount(1);
    
    // Subscribe to StarRatingChanged events
    int eventCount = 0;
    StarRating lastOldRating = StarRating::Star1;
    StarRating lastNewRating = StarRating::Star1;
    float lastAvgSatisfaction = 0.0f;
    
    auto subId = eventBus.subscribe(EventType::StarRatingChanged, 
        [&](const Event& e) {
            ++eventCount;
            auto payload = std::any_cast<StarRatingChangedPayload>(e.payload);
            lastOldRating = payload.oldRating;
            lastNewRating = payload.newRating;
            lastAvgSatisfaction = payload.avgSatisfaction;
        });
    
    SECTION("StarRatingChanged event is emitted when rating changes") {
        // Initial: Star1 (default)
        mockAgents.setAverageSatisfaction(40.0f); // Should be Star2
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        // Flush events
        eventBus.flush();
        
        REQUIRE(eventCount == 1);
        REQUIRE(lastOldRating == StarRating::Star1);
        REQUIRE(lastNewRating == StarRating::Star2);
        REQUIRE_THAT(lastAvgSatisfaction, WithinRel(40.0f, 0.01f));
    }
    
    SECTION("StarRatingChanged event is NOT emitted when rating stays the same") {
        // Set to Star2 first
        mockAgents.setAverageSatisfaction(40.0f);
        GameTime time1{0, 0, 0};
        system.update(reg, mockAgents, time1);
        eventBus.flush();
        eventCount = 0; // Reset after first change
        
        // Update with same satisfaction (should stay Star2)
        GameTime time2{0, 0, 1};
        system.update(reg, mockAgents, time2);
        eventBus.flush();
        
        REQUIRE(eventCount == 0); // No event for same rating
    }
    
    eventBus.unsubscribe(subId);
}

TEST_CASE("StarRatingSystem edge cases", "[StarRatingSystem]") {
    EventBus eventBus;
    StarRatingSystem::Config config;
    config.satisfactionThresholds = {0.f, 30.f, 50.f, 70.f, 90.f};
    config.minPopulationForStar2 = 2; // Test with different minimum
    StarRatingSystem system(eventBus, config);
    
    entt::registry reg;
    system.initRegistry(reg);
    
    MockAgentSystem mockAgents;
    
    SECTION("Rating stays Star1 when population < minPopulationForStar2") {
        mockAgents.setActiveAgentCount(1); // Below minimum of 2
        mockAgents.setAverageSatisfaction(100.0f); // Perfect satisfaction
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star1);
    }
    
    SECTION("Rating updates when population reaches minPopulationForStar2") {
        mockAgents.setActiveAgentCount(2); // Exactly minimum
        mockAgents.setAverageSatisfaction(80.0f); // Should be Star4
        
        GameTime time{0, 0, 0};
        system.update(reg, mockAgents, time);
        
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star4);
    }
    
    SECTION("Exact threshold values") {
        // Need at least minPopulationForStar2 (2) to get above Star1
        mockAgents.setActiveAgentCount(2);
        
        // Exactly at threshold should give that star
        mockAgents.setAverageSatisfaction(30.0f);
        GameTime time1{0, 0, 0};
        system.update(reg, mockAgents, time1);
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star2);
        
        // Just below threshold
        mockAgents.setAverageSatisfaction(29.9f);
        GameTime time2{0, 0, 1};
        system.update(reg, mockAgents, time2);
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star1);
        
        // Just above next threshold
        mockAgents.setAverageSatisfaction(50.1f);
        GameTime time3{0, 0, 2};
        system.update(reg, mockAgents, time3);
        REQUIRE(system.getCurrentRating(reg) == StarRating::Star3);
    }
}

} // namespace vse