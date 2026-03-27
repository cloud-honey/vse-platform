/**
 * @file test_GameOver.cpp
 * @layer Layer 1 Test
 * @task TASK-04-004
 * @author Boom2 (DeepSeek)
 *
 * @brief Unit tests for GameOverSystem.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "domain/GameOverSystem.h"
#include "core/EventBus.h"
#include "core/IGridSystem.h"
#include "core/IAgentSystem.h"
#include <entt/entt.hpp>
#include <memory>

using Catch::Matchers::Equals;

namespace vse {
namespace {

// Mock IGridSystem for testing
class MockGridSystem : public IGridSystem {
public:
    MockGridSystem() = default;
    
    void setBuiltFloorCount(int count) { builtFloorCount_ = count; }
    
    // IGridSystem interface
    int maxFloors() const override { return 100; }
    int floorWidth() const override { return 20; }
    Result<bool> buildFloor(int floor) override {
        (void)floor;
        return Result<bool>::success(true);
    }
    bool isFloorBuilt(int floor) const override {
        (void)floor;
        return true;
    }
    int builtFloorCount() const override {
        return builtFloorCount_;
    }
    Result<bool> placeTenant(TileCoord anchor, TenantType type, int width, EntityId entity) override {
        (void)anchor; (void)type; (void)width; (void)entity;
        return Result<bool>::success(true);
    }
    Result<bool> removeTenant(TileCoord anyTile) override {
        (void)anyTile;
        return Result<bool>::success(true);
    }
    std::optional<TileData> getTile(TileCoord pos) const override {
        (void)pos;
        return std::nullopt;
    }
    bool isTileEmpty(TileCoord pos) const override {
        (void)pos;
        return true;
    }
    bool isValidCoord(TileCoord pos) const override {
        (void)pos;
        return true;
    }
    std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const override {
        (void)floor;
        return {};
    }
    Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) override {
        (void)x; (void)bottomFloor; (void)topFloor;
        return Result<bool>::success(true);
    }
    bool isElevatorShaft(TileCoord pos) const override {
        (void)pos;
        return false;
    }
    std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const override {
        (void)from; (void)searchRadius;
        return std::nullopt;
    }
    int getTenantCount() const override { return 0; }
    
private:
    int builtFloorCount_ = 0;
};

// Mock IAgentSystem for testing
class MockAgentSystem : public IAgentSystem {
public:
    MockAgentSystem() = default;
    
    void setActiveAgentCount(int count) { activeCount_ = count; }
    
    // IAgentSystem interface
    Result<EntityId> spawnAgent(entt::registry& reg,
                                EntityId homeTenantId,
                                EntityId workplaceId,
                                std::optional<TileCoord> spawnPos = std::nullopt) override {
        (void)reg; (void)homeTenantId; (void)workplaceId; (void)spawnPos;
        return Result<EntityId>::success(INVALID_ENTITY);
    }
    
    void despawnAgent(entt::registry& reg, EntityId id) override {
        (void)reg; (void)id;
    }
    
    int activeAgentCount() const override {
        return activeCount_;
    }
    
    void update(entt::registry& reg, const GameTime& time) override {
        (void)reg; (void)time;
    }
    
    std::optional<AgentState> getState(entt::registry& reg, EntityId id) const override {
        (void)reg; (void)id;
        return std::nullopt;
    }
    
    std::vector<EntityId> getAgentsOnFloor(entt::registry& reg, int floor) const override {
        (void)reg; (void)floor;
        return {};
    }
    
    std::vector<EntityId> getAgentsInState(entt::registry& reg, AgentState state) const override {
        (void)reg; (void)state;
        return {};
    }
    
    float getAverageSatisfaction(const entt::registry& reg) const override {
        (void)reg;
        return 50.0f;
    }
    
private:
    int activeCount_ = 0;
};

} // anonymous namespace

TEST_CASE("GameOverSystem - No game over before 30 days of negative balance", "[GameOverSystem]") {
    EventBus bus;
    MockGridSystem grid;
    MockAgentSystem agents;
    GameOverSystem gameOver(grid, agents, bus);
    
    entt::registry reg;
    GameTime time{0, 0, 0};
    
    // Test 29 days of negative balance
    for (int day = 0; day < 29; ++day) {
        time.day = day;
        gameOver.update(time, -1000, 1); // Negative balance
        REQUIRE_FALSE(gameOver.isGameOver());
    }
}

TEST_CASE("GameOverSystem - Game over fires exactly at 30th consecutive negative day", "[GameOverSystem]") {
    EventBus bus;
    MockGridSystem grid;
    MockAgentSystem agents;
    GameOverSystem gameOver(grid, agents, bus);
    
    entt::registry reg;
    GameTime time{0, 0, 0};
    
    bool gameOverFired = false;
    bus.subscribe(EventType::GameOver, [&](const Event& e) {
        gameOverFired = true;
        REQUIRE(e.payload.has_value());
        auto payload = std::any_cast<GameOverPayload>(e.payload);
        REQUIRE(payload.reason == "bankruptcy");
        REQUIRE(payload.day == 29); // 0-indexed, so day 29 is the 30th day
        REQUIRE(payload.finalBalance == -1000);
    });
    
    // 30 days of negative balance
    for (int day = 0; day < 30; ++day) {
        time.day = day;
        gameOver.update(time, -1000, 1);
        bus.flush(); // Flush events after each update
    }
    
    REQUIRE(gameOverFired);
    REQUIRE(gameOver.isGameOver());
}

TEST_CASE("GameOverSystem - Counter resets when balance goes positive", "[GameOverSystem]") {
    EventBus bus;
    MockGridSystem grid;
    MockAgentSystem agents;
    GameOverSystem gameOver(grid, agents, bus);
    
    entt::registry reg;
    GameTime time{0, 0, 0};
    
    // 15 days negative
    for (int day = 0; day < 15; ++day) {
        time.day = day;
        gameOver.update(time, -1000, 1);
    }
    
    // 1 day positive (should reset counter)
    time.day = 15;
    gameOver.update(time, 1000, 1);
    
    // 15 more days negative (total 30, but counter was reset)
    for (int day = 16; day < 31; ++day) {
        time.day = day;
        gameOver.update(time, -1000, 1);
        REQUIRE_FALSE(gameOver.isGameOver());
    }
}

TEST_CASE("GameOverSystem - Game over fires only once (guard)", "[GameOverSystem]") {
    EventBus bus;
    MockGridSystem grid;
    MockAgentSystem agents;
    GameOverSystem gameOver(grid, agents, bus);
    
    entt::registry reg;
    GameTime time{0, 0, 0};
    
    int gameOverCount = 0;
    bus.subscribe(EventType::GameOver, [&](const Event& e) {
        gameOverCount++;
    });
    
    // Trigger game over
    for (int day = 0; day < 30; ++day) {
        time.day = day;
        gameOver.update(time, -1000, 1);
        bus.flush(); // Flush events after each update
    }
    
    REQUIRE(gameOverCount == 1);
    
    // Try to trigger again (should not fire)
    for (int day = 30; day < 60; ++day) {
        time.day = day;
        gameOver.update(time, -1000, 1);
        bus.flush(); // Flush events after each update
    }
    
    REQUIRE(gameOverCount == 1); // Still only 1
}

TEST_CASE("GameOverSystem - TOWER victory fires when all 3 conditions met", "[GameOverSystem]") {
    EventBus bus;
    MockGridSystem grid;
    MockAgentSystem agents;
    GameOverSystem gameOver(grid, agents, bus);
    
    entt::registry reg;
    GameTime time{0, 0, 0};
    
    // Set up victory conditions
    grid.setBuiltFloorCount(100);
    agents.setActiveAgentCount(300);
    
    bool victoryFired = false;
    bus.subscribe(EventType::TowerAchieved, [&](const Event& e) {
        victoryFired = true;
        REQUIRE(e.payload.has_value());
        auto payload = std::any_cast<TowerAchievedPayload>(e.payload);
        REQUIRE(payload.day >= 89); // fires after 90 consecutive positive days
        REQUIRE(payload.starRating == 5);
        REQUIRE(payload.floorCount == 100);
        REQUIRE(payload.npcCount == 300);
    });
    
    // Must have 90 consecutive positive-balance days — spec §5.21
    for (int day = 0; day <= 90; ++day) {
        GameTime t{day, 0, 0};
        gameOver.update(t, 1000000, 5);
        bus.flush();
        if (victoryFired) break;
    }

    REQUIRE(victoryFired);
    REQUIRE(gameOver.isVictoryAchieved());
}

TEST_CASE("GameOverSystem - TOWER victory does NOT fire if any condition missing", "[GameOverSystem]") {
    EventBus bus;
    MockGridSystem grid;
    MockAgentSystem agents;
    GameOverSystem gameOver(grid, agents, bus);
    
    entt::registry reg;
    GameTime time{0, 0, 0};
    
    int victoryCount = 0;
    bus.subscribe(EventType::TowerAchieved, [&](const Event& e) {
        victoryCount++;
    });
    
    // Test 1: Star rating < 5
    SECTION("Star rating < 5") {
        grid.setBuiltFloorCount(100);
        agents.setActiveAgentCount(300);
        gameOver.update(time, 1000000, 4); // Only 4 stars
        REQUIRE(victoryCount == 0);
        REQUIRE_FALSE(gameOver.isVictoryAchieved());
    }
    
    // Test 2: Floors < 100
    SECTION("Floors < 100") {
        grid.setBuiltFloorCount(99);
        agents.setActiveAgentCount(300);
        gameOver.update(time, 1000000, 5);
        REQUIRE(victoryCount == 0);
        REQUIRE_FALSE(gameOver.isVictoryAchieved());
    }
    
    // Test 3: NPCs < 300
    SECTION("NPCs < 300") {
        grid.setBuiltFloorCount(100);
        agents.setActiveAgentCount(299);
        gameOver.update(time, 1000000, 5);
        REQUIRE(victoryCount == 0);
        REQUIRE_FALSE(gameOver.isVictoryAchieved());
    }
}

TEST_CASE("GameOverSystem - Reset functionality", "[GameOverSystem]") {
    EventBus bus;
    MockGridSystem grid;
    MockAgentSystem agents;
    GameOverSystem gameOver(grid, agents, bus);
    
    entt::registry reg;
    GameTime time{0, 0, 0};
    
    // Trigger game over
    for (int day = 0; day < 30; ++day) {
        time.day = day;
        gameOver.update(time, -1000, 1);
    }
    
    REQUIRE(gameOver.isGameOver());
    
    // Reset
    gameOver.reset();
    
    REQUIRE_FALSE(gameOver.isGameOver());
    REQUIRE_FALSE(gameOver.isVictoryAchieved());
    
    // Should be able to trigger again after reset
    for (int day = 0; day < 30; ++day) {
        time.day = day;
        gameOver.update(time, -1000, 1);
        bus.flush(); // Flush events after each update
    }
    
    REQUIRE(gameOver.isGameOver());
}

} // namespace vse