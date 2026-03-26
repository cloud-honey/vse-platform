/**
 * @file test_EconomyEngine.cpp
 * @layer Layer 1 Test
 * @task TASK-02-003
 * @author DeepSeek V3
 * @reviewed pending
 *
 * @brief Unit tests for EconomyEngine.
 */

#include "domain/EconomyEngine.h"
#include "domain/GridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace vse;

// Test helper
static EconomyConfig makeTestConfig() {
    return EconomyConfig{
        1000000,   // startingBalance
        500,       // officeRentPerTilePerDay
        300,       // residentialRentPerTilePerDay
        800,       // commercialRentPerTilePerDay
        1000       // elevatorMaintenancePerDay
    };
}

// Test helper to create EconomyEngine with mock EventBus
static EconomyEngine createTestEngine(const EconomyConfig& config) {
    static EventBus mockEventBus;
    return EconomyEngine(config, mockEventBus);
}

// Mock GridSystem for testing
class MockGridSystem : public IGridSystem {
public:
    MockGridSystem() : maxFloors_(30), floorWidth_(20) {}
    
    void addTenant(TileCoord anchor, TenantType type, int width, EntityId entity) {
        tenants_.push_back({anchor, type, width, entity});
    }
    
    void addElevatorShaft(TileCoord anchor) {
        elevatorShafts_.push_back(anchor);
    }
    
    // IGridSystem implementation
    int maxFloors() const override { return maxFloors_; }
    int floorWidth() const override { return floorWidth_; }
    
    Result<bool> buildFloor(int floor) override { return Result<bool>::success(true); }
    bool isFloorBuilt(int floor) const override { return floor >= 0 && floor < maxFloors_; }
    int builtFloorCount() const override { return maxFloors_; }
    int getTenantCount() const override { return static_cast<int>(tenants_.size()); }
    
    Result<bool> placeTenant(TileCoord anchor, TenantType type, int width, EntityId entity) override {
        return Result<bool>::success(true);
    }
    
    Result<bool> removeTenant(TileCoord anyTile) override {
        return Result<bool>::success(true);
    }
    
    std::optional<TileData> getTile(TileCoord pos) const override {
        for (const auto& tenant : tenants_) {
            if (pos.x >= tenant.anchor.x && pos.x < tenant.anchor.x + tenant.width && 
                pos.floor == tenant.anchor.floor) {
                TileData data;
                data.tenantType = tenant.type;
                data.tenantEntity = tenant.entity;
                data.isAnchor = (pos.x == tenant.anchor.x);
                data.tileWidth = tenant.width;
                return data;
            }
        }
        
        for (const auto& shaft : elevatorShafts_) {
            if (pos.x == shaft.x && pos.floor == shaft.floor) {
                TileData data;
                data.isElevatorShaft = true;
                data.isAnchor = true;
                return data;
            }
        }
        
        return std::nullopt;
    }
    
    bool isTileEmpty(TileCoord pos) const override {
        return !getTile(pos).has_value();
    }
    
    bool isValidCoord(TileCoord pos) const override {
        return pos.x >= 0 && pos.x < floorWidth_ && pos.floor >= 0 && pos.floor < maxFloors_;
    }
    
    std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const override {
        std::vector<std::pair<TileCoord, TileData>> result;
        
        for (const auto& tenant : tenants_) {
            if (tenant.anchor.floor == floor) {
                for (int x = tenant.anchor.x; x < tenant.anchor.x + tenant.width; ++x) {
                    TileCoord coord{x, floor};
                    TileData data;
                    data.tenantType = tenant.type;
                    data.tenantEntity = tenant.entity;
                    data.isAnchor = (x == tenant.anchor.x);
                    data.tileWidth = tenant.width;
                    result.push_back({coord, data});
                }
            }
        }
        
        for (const auto& shaft : elevatorShafts_) {
            if (shaft.floor == floor) {
                TileCoord coord{shaft.x, floor};
                TileData data;
                data.isElevatorShaft = true;
                data.isAnchor = true;
                result.push_back({coord, data});
            }
        }
        
        return result;
    }
    
    Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) override {
        return Result<bool>::success(true);
    }
    
    bool isElevatorShaft(TileCoord pos) const override {
        for (const auto& shaft : elevatorShafts_) {
            if (pos.x == shaft.x && pos.floor == shaft.floor) {
                return true;
            }
        }
        return false;
    }
    
    std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const override {
        return std::nullopt;
    }
    
private:
    struct TenantInfo {
        TileCoord anchor;
        TenantType type;
        int width;
        EntityId entity;
    };
    
    int maxFloors_;
    int floorWidth_;
    std::vector<TenantInfo> tenants_;
    std::vector<TileCoord> elevatorShafts_;
};

TEST_CASE("EconomyEngine - Initial balance equals config.startingBalance", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    REQUIRE(engine.getBalance() == config.startingBalance);
}

TEST_CASE("EconomyEngine - addIncome increases balance correctly", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    engine.addIncome(EntityId{100}, TenantType::Office, 5000, time);
    
    REQUIRE(engine.getBalance() == config.startingBalance + 5000);
    REQUIRE(engine.getDailyIncome() == 5000);
}

TEST_CASE("EconomyEngine - addExpense decreases balance correctly", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    engine.addExpense("maintenance", 3000, time);
    
    REQUIRE(engine.getBalance() == config.startingBalance - 3000);
    REQUIRE(engine.getDailyExpense() == 3000);
}

TEST_CASE("EconomyEngine - Balance can go negative (debt allowed)", "[EconomyEngine]") {
    auto config = makeTestConfig();
    config.startingBalance = 1000;
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    engine.addExpense("construction", 2000, time);
    
    REQUIRE(engine.getBalance() == -1000);
}

TEST_CASE("EconomyEngine - getRecentIncome returns newest first, capped at requested count", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    engine.addIncome(EntityId{1}, TenantType::Office, 100, time);
    engine.addIncome(EntityId{2}, TenantType::Residential, 200, time);
    engine.addIncome(EntityId{3}, TenantType::Commercial, 300, time);
    
    auto recent = engine.getRecentIncome(2);
    REQUIRE(recent.size() == 2);
    REQUIRE(recent[0].amount == 300);  // Newest first
    REQUIRE(recent[1].amount == 200);
}

TEST_CASE("EconomyEngine - getRecentExpenses returns newest first, capped at requested count", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    engine.addExpense("maintenance", 100, time);
    engine.addExpense("construction", 200, time);
    engine.addExpense("elevator", 300, time);
    
    auto recent = engine.getRecentExpenses(2);
    REQUIRE(recent.size() == 2);
    REQUIRE(recent[0].amount == 300);  // Newest first
    REQUIRE(recent[1].amount == 200);
}

TEST_CASE("EconomyEngine - History capped at 100 entries", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    // Add 110 income records
    for (int i = 0; i < 110; ++i) {
        engine.addIncome(EntityId{static_cast<uint32_t>(i)}, TenantType::Office, 100 + i, time);
    }
    
    auto recent = engine.getRecentIncome(200);  // Request more than we have
    REQUIRE(recent.size() == 100);  // Capped at 100
}

TEST_CASE("EconomyEngine - collectRent adds income for office tenant", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    MockGridSystem grid;
    
    // Add an office tenant with width 3
    grid.addTenant({5, 1}, TenantType::Office, 3, EntityId{100});
    
    GameTime time{1, 0, 0};  // Day 1
    engine.collectRent(grid, time);
    
    // Office rent: 500 per tile per day * 3 tiles = 1500
    REQUIRE(engine.getBalance() == config.startingBalance + 1500);
    REQUIRE(engine.getDailyIncome() == 1500);
}

TEST_CASE("EconomyEngine - collectRent for residential tenant", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    MockGridSystem grid;
    
    // Add a residential tenant with width 2
    grid.addTenant({8, 2}, TenantType::Residential, 2, EntityId{200});
    
    GameTime time{1, 0, 0};
    engine.collectRent(grid, time);
    
    // Residential rent: 300 per tile per day * 2 tiles = 600
    REQUIRE(engine.getBalance() == config.startingBalance + 600);
}

TEST_CASE("EconomyEngine - collectRent for commercial tenant", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    MockGridSystem grid;
    
    // Add a commercial tenant with width 4
    grid.addTenant({3, 3}, TenantType::Commercial, 4, EntityId{300});
    
    GameTime time{1, 0, 0};
    engine.collectRent(grid, time);
    
    // Commercial rent: 800 per tile per day * 4 tiles = 3200
    REQUIRE(engine.getBalance() == config.startingBalance + 3200);
}

TEST_CASE("EconomyEngine - collectRent is idempotent per day", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    MockGridSystem grid;
    
    grid.addTenant({5, 1}, TenantType::Office, 2, EntityId{100});
    
    GameTime time{1, 0, 0};
    engine.collectRent(grid, time);
    int64_t balanceAfterFirst = engine.getBalance();
    
    // Call collectRent again on same day
    GameTime sameDay{1, 12, 0};  // Same day, different time
    engine.collectRent(grid, sameDay);
    
    // Balance should not change
    REQUIRE(engine.getBalance() == balanceAfterFirst);
}

TEST_CASE("EconomyEngine - payMaintenance charges per elevator shaft", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    MockGridSystem grid;
    
    // Add 3 elevator shafts
    grid.addElevatorShaft({2, 0});
    grid.addElevatorShaft({3, 0});
    grid.addElevatorShaft({4, 0});
    
    GameTime time{1, 0, 0};
    engine.payMaintenance(grid, time);
    
    // Maintenance: 1000 per shaft per day * 3 shafts = 3000
    REQUIRE(engine.getBalance() == config.startingBalance - 3000);
    REQUIRE(engine.getDailyExpense() == 3000);
}

TEST_CASE("EconomyEngine - payMaintenance is idempotent per day", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    MockGridSystem grid;
    
    grid.addElevatorShaft({2, 0});
    
    GameTime time{1, 0, 0};
    engine.payMaintenance(grid, time);
    int64_t balanceAfterFirst = engine.getBalance();
    
    // Call payMaintenance again on same day
    GameTime sameDay{1, 12, 0};
    engine.payMaintenance(grid, sameDay);
    
    // Balance should not change
    REQUIRE(engine.getBalance() == balanceAfterFirst);
}

TEST_CASE("EconomyEngine - dailyIncome resets on new day", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    GameTime day1{1, 12, 0};
    engine.addIncome(EntityId{100}, TenantType::Office, 5000, day1);
    REQUIRE(engine.getDailyIncome() == 5000);
    
    // Update to next day (hour 0, minute 0)
    GameTime day2Start{2, 0, 0};
    engine.update(day2Start);
    
    REQUIRE(engine.getDailyIncome() == 0);
}

TEST_CASE("EconomyEngine - dailyExpense resets on new day", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    GameTime day1{1, 12, 0};
    engine.addExpense("maintenance", 3000, day1);
    REQUIRE(engine.getDailyExpense() == 3000);
    
    // Update to next day (hour 0, minute 0)
    GameTime day2Start{2, 0, 0};
    engine.update(day2Start);
    
    REQUIRE(engine.getDailyExpense() == 0);
}

TEST_CASE("EconomyEngine - Overflow guard: addIncome near INT64_MAX clamps and logs warning", "[EconomyEngine]") {
    auto config = makeTestConfig();
    config.startingBalance = INT64_MAX - 100;
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    engine.addIncome(EntityId{100}, TenantType::Office, 200, time);
    
    // Should clamp to INT64_MAX
    REQUIRE(engine.getBalance() == INT64_MAX);
}

TEST_CASE("EconomyEngine - Underflow guard: addExpense near INT64_MIN clamps", "[EconomyEngine]") {
    auto config = makeTestConfig();
    config.startingBalance = INT64_MIN + 100;
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    engine.addExpense("construction", 200, time);
    
    // Should clamp to INT64_MIN
    REQUIRE(engine.getBalance() == INT64_MIN);
}

TEST_CASE("EconomyEngine - Non-positive amounts are ignored", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    
    GameTime time{0, 12, 0};
    int64_t initialBalance = engine.getBalance();
    
    engine.addIncome(EntityId{100}, TenantType::Office, 0, time);
    engine.addIncome(EntityId{101}, TenantType::Office, -100, time);
    engine.addExpense("maintenance", 0, time);
    engine.addExpense("construction", -50, time);
    
    REQUIRE(engine.getBalance() == initialBalance);
    REQUIRE(engine.getDailyIncome() == 0);
    REQUIRE(engine.getDailyExpense() == 0);
}

TEST_CASE("EconomyEngine - collectRent and payMaintenance both run on same day", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    MockGridSystem grid;
    
    // Add a tenant and an elevator shaft
    grid.addTenant({5, 1}, TenantType::Office, 2, EntityId{100});
    grid.addElevatorShaft({2, 0});
    
    GameTime time{1, 0, 0};
    
    // Both should run on the same day
    engine.collectRent(grid, time);
    engine.payMaintenance(grid, time);
    
    // Office rent: 500 per tile per day * 2 tiles = 1000
    // Maintenance: 1000 per shaft per day * 1 shaft = 1000
    // Starting balance: 1,000,000 + 1000 - 1000 = 1,000,000
    REQUIRE(engine.getBalance() == 1000000);
    REQUIRE(engine.getDailyIncome() == 1000);
    REQUIRE(engine.getDailyExpense() == 1000);
}

TEST_CASE("EconomyEngine - Neither rent nor maintenance runs twice on same day", "[EconomyEngine]") {
    auto config = makeTestConfig();
    EconomyEngine engine = createTestEngine(config);
    MockGridSystem grid;
    
    grid.addTenant({5, 1}, TenantType::Office, 1, EntityId{100});
    grid.addElevatorShaft({2, 0});
    
    GameTime time{1, 0, 0};
    
    // Run both twice
    engine.collectRent(grid, time);
    engine.payMaintenance(grid, time);
    int64_t balanceAfterFirst = engine.getBalance();
    
    GameTime sameDay{1, 12, 0};
    engine.collectRent(grid, sameDay);
    engine.payMaintenance(grid, sameDay);
    
    // Balance should not change
    REQUIRE(engine.getBalance() == balanceAfterFirst);
}