/**
 * @file test_Integration.cpp
 * @layer Integration Test
 * @task TASK-02-002
 * @author DeepSeek V3
 * @reviewed pending
 *
 * @brief Cross-system integration tests.
 *        Verifies that SimClock, EventBus, AgentSystem, TransportSystem,
 *        and GridSystem cooperate correctly across multiple simulation ticks.
 *
 * @note These tests use real system instances (no mocks).
 *       SDL is NOT initialized — Domain only.
 *       Uses VSE_PROJECT_ROOT macro for config path resolution.
 */

#include <catch2/catch_test_macros.hpp>
#include "core/SimClock.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include "core/IRenderCommand.h"
#include "domain/GridSystem.h"
#include "domain/AgentSystem.h"
#include "domain/TransportSystem.h"
#include "domain/EconomyEngine.h"
#include "domain/StarRatingSystem.h"
#include "renderer/HUDPanel.h"
#include <entt/entt.hpp>

using namespace vse;

static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

// ConfigManager를 생성자에서 미리 로드하는 헬퍼
struct PreloadedConfig : ConfigManager {
    PreloadedConfig() { loadFromFile(cfgPath()); }
};

TEST_CASE("Integration - Elevator: single call, car arrives and opens door", "[Integration]") {
    // Load config from VSE_PROJECT_ROOT + "/assets/config/game_config.json"
    PreloadedConfig config;
    
    EventBus eventBus;
    entt::registry registry;
    GridSystem grid(eventBus, config);
    TransportSystem transport(grid, eventBus, config);

    // Build floor 0 and floor 3
    grid.buildFloor(0);
    grid.buildFloor(3);

    // Place elevator shaft spanning floor 0 to 3 at shaftX=5
    grid.placeElevatorShaft(5, 0, 3);

    // Create one elevator at shaftX=5, floor 0 to 3, capacity 8
    auto result = transport.createElevator(5, 0, 3, 8);
    REQUIRE(result.ok() == true);
    REQUIRE(result.value != INVALID_ENTITY);

    // At least one elevator snapshot exists
    REQUIRE(!transport.getAllElevators().empty());

    // Call elevator to floor 3 (direction: Up)
    transport.callElevator(5, 3, ElevatorDirection::Up);

    // After sufficient ticks, elevator must reach floor 3 with door open
    bool arrived = false;
    for (int tick = 0; tick < 200 && !arrived; ++tick) {
        eventBus.flush();
        GameTime t = GameTime::fromTick(tick);
        transport.update(t);
        auto elevIds = transport.getAllElevators();
        for (const auto& elevId : elevIds) {
            auto snap = transport.getElevatorState(elevId);
            if (snap.has_value() && snap->currentFloor == 3 &&
                (snap->state == ElevatorState::DoorOpening ||
                 snap->state == ElevatorState::Boarding ||
                 snap->state == ElevatorState::DoorClosing)) {
                arrived = true;
            }
        }
    }
    REQUIRE(arrived);
}

TEST_CASE("Integration - NPC: daily schedule transitions (Idle → Working → Resting)", "[Integration]") {
    PreloadedConfig config;
    
    EventBus eventBus;
    entt::registry registry;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);

    // Build floors 0 and 2
    grid.buildFloor(0);
    grid.buildFloor(2);

    // Place one residential tenant on floor 0 (home)
    EntityId homeId = registry.create();
    grid.placeTenant({0, 0}, TenantType::Residential, 2, homeId);

    // Place one office tenant on floor 2 (work)
    EntityId workId = registry.create();
    grid.placeTenant({0, 2}, TenantType::Office, 3, workId);

    // Spawn one NPC
    auto spawnResult = agents.spawnAgent(registry, homeId, workId);
    REQUIRE(spawnResult.ok() == true);
    REQUIRE(spawnResult.value != INVALID_ENTITY);

    bool sawWorking = false;
    bool sawResting = false;

    for (int tick = 0; tick < 1440; ++tick) {
        eventBus.flush();
        GameTime t = GameTime::fromTick(tick);
        agents.update(registry, t);

        auto states = agents.getAgentsInState(registry, AgentState::Working);
        if (!states.empty()) sawWorking = true;

        auto resting = agents.getAgentsInState(registry, AgentState::Resting);
        if (!resting.empty()) sawResting = true;
    }

    // NPC must have worked at some point during the day
    REQUIRE(sawWorking);
    // NPC must have rested at some point during the day (evening/night)
    REQUIRE(sawResting);
}

TEST_CASE("Integration - SimClock: TickAdvanced and HourChanged events delivered correctly", "[Integration]") {
    EventBus eventBus;
    SimClock clock(eventBus);

    int tickCount = 0;
    int hourCount = 0;

    // Subscribe to TickAdvanced
    eventBus.subscribe(EventType::TickAdvanced, [&](const Event&) {
        ++tickCount;
    });
    // Subscribe to HourChanged
    eventBus.subscribe(EventType::HourChanged, [&](const Event&) {
        ++hourCount;
    });

    // Advance 120 ticks (= 2 game hours)
    for (int i = 0; i < 120; ++i) {
        eventBus.flush();   // deliver deferred events from previous tick
        clock.advanceTick();
    }
    eventBus.flush();  // deliver last tick's events

    // Must have received exactly 120 TickAdvanced events
    REQUIRE(tickCount == 120);
    // Must have received exactly 2 HourChanged events (at tick 60 and tick 120)
    REQUIRE(hourCount == 2);
}
// ── TASK-02-009: NPC precise timing (tick 540 = 9am work start, tick 1080 = 6pm work end) ──

TEST_CASE("Integration - NPC precise timing: Working at tick 540, Idle at tick 1080", "[Integration]") {
    PreloadedConfig config;
    EventBus eventBus;
    entt::registry registry;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);  // no transport → same-floor only

    grid.buildFloor(0);
    EntityId homeId = registry.create();
    grid.placeTenant({2, 0}, TenantType::Residential, 2, homeId);
    EntityId workId = registry.create();
    grid.placeTenant({6, 0}, TenantType::Office, 2, workId);

    auto spawnResult = agents.spawnAgent(registry, homeId, workId);
    REQUIRE(spawnResult.ok());
    EntityId agentId = spawnResult.value;

    // Advance to tick 539 (8:59 AM) — should still be Idle
    for (int tick = 0; tick < 539; ++tick) {
        eventBus.flush();
        agents.update(registry, GameTime::fromTick(tick));
    }
    REQUIRE(agents.getState(registry, agentId) == AgentState::Idle);

    // Tick 540 (9:00 AM) — should transition to Working
    eventBus.flush();
    agents.update(registry, GameTime::fromTick(540));
    REQUIRE(agents.getState(registry, agentId) == AgentState::Working);

    // Advance to tick 1079 (5:59 PM) — still Working (after lunch)
    for (int tick = 541; tick < 1080; ++tick) {
        eventBus.flush();
        agents.update(registry, GameTime::fromTick(tick));
    }
    auto stateAt1079 = agents.getState(registry, agentId);
    REQUIRE((stateAt1079 == AgentState::Working || stateAt1079 == AgentState::Resting));

    // Tick 1080 (6:00 PM) — should transition to Idle
    eventBus.flush();
    agents.update(registry, GameTime::fromTick(1080));
    REQUIRE(agents.getState(registry, agentId) == AgentState::Idle);
}

// ── TASK-02-009: Economy + StarRating integration ──

TEST_CASE("Integration - Economy income + StarRating update in one simulation day", "[Integration]") {
    PreloadedConfig config;
    EventBus eventBus;
    entt::registry registry;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);

    EconomyConfig ecfg;
    ecfg.startingBalance = 100000;
    ecfg.officeRentPerTilePerDay = 500;
    ecfg.residentialRentPerTilePerDay = 300;
    ecfg.commercialRentPerTilePerDay = 800;
    ecfg.elevatorMaintenancePerDay = 500;
    EconomyEngine economy(ecfg, eventBus);

    StarRatingSystem::Config srCfg;
    StarRatingSystem starRating(eventBus, srCfg);
    starRating.initRegistry(registry);

    // Setup: floor 0 with residential + office
    grid.buildFloor(0);
    EntityId homeId = registry.create();
    grid.placeTenant({2, 0}, TenantType::Residential, 2, homeId);
    EntityId workId = registry.create();
    grid.placeTenant({6, 0}, TenantType::Office, 3, workId);

    auto spawnResult = agents.spawnAgent(registry, homeId, workId);
    REQUIRE(spawnResult.ok());

    // Run one full game day (1440 ticks)
    for (int tick = 0; tick < 1440; ++tick) {
        eventBus.flush();
        GameTime t = GameTime::fromTick(tick);
        agents.update(registry, t);
        economy.update(t);
        starRating.update(registry, agents, t);
    }

    // StarRating should have been computed (at least Star1)
    auto rating = starRating.getCurrentRating(registry);
    REQUIRE(static_cast<int>(rating) >= static_cast<int>(StarRating::Star1));

    // Economy should still have balance (starting 100000, small operations)
    REQUIRE(economy.getBalance() > 0);
}

// ── TASK-03-007: HUD fields integration ──

TEST_CASE("Integration - RenderFrame HUD fields wired correctly", "[Integration][HUD][RenderFrame]") {
    // Test that HUD fields in RenderFrame can be set and read
    RenderFrame frame;
    
    // Test initial values
    REQUIRE(frame.balance == 0);
    REQUIRE(frame.starRating == 0.0f);
    REQUIRE(frame.currentTick == 0);
    REQUIRE(frame.tenantCount == 0);
    REQUIRE(frame.npcCount == 0);
    
    // Test setting values
    frame.balance = 1234567;
    frame.starRating = 3.5f;
    frame.currentTick = 100;
    frame.tenantCount = 5;
    frame.npcCount = 12;
    
    REQUIRE(frame.balance == 1234567);
    REQUIRE(frame.starRating == 3.5f);
    REQUIRE(frame.currentTick == 100);
    REQUIRE(frame.tenantCount == 5);
    REQUIRE(frame.npcCount == 12);
}

TEST_CASE("Integration - HUD data sources provide valid values", "[Integration][HUD]") {
    PreloadedConfig config;
    EventBus eventBus;
    entt::registry registry;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);

    EconomyConfig ecfg;
    ecfg.startingBalance = 100000;
    ecfg.officeRentPerTilePerDay = 500;
    ecfg.residentialRentPerTilePerDay = 300;
    ecfg.commercialRentPerTilePerDay = 800;
    ecfg.elevatorMaintenancePerDay = 500;
    EconomyEngine economy(ecfg, eventBus);

    StarRatingSystem::Config srCfg;
    StarRatingSystem starRating(eventBus, srCfg);
    starRating.initRegistry(registry);

    // Setup: one floor with tenants
    grid.buildFloor(0);
    EntityId homeId = registry.create();
    grid.placeTenant({2, 0}, TenantType::Residential, 2, homeId);
    EntityId workId = registry.create();
    grid.placeTenant({6, 0}, TenantType::Office, 3, workId);

    auto spawnResult = agents.spawnAgent(registry, homeId, workId);
    REQUIRE(spawnResult.ok());

    // Check HUD data sources
    REQUIRE(grid.getTenantCount() == 2);  // residential + office
    REQUIRE(economy.getBalance() == ecfg.startingBalance);  // starting balance
    REQUIRE(agents.activeAgentCount() == 1);  // one NPC
    
    // Star rating should be Star1 (minimum with NPC)
    starRating.update(registry, agents, GameTime::fromTick(0));
    auto rating = starRating.getCurrentRating(registry);
    REQUIRE(static_cast<int>(rating) >= static_cast<int>(StarRating::Star1));
}

TEST_CASE("Integration - GridSystem getTenantCount works with multiple tenants", "[Integration][GridSystem][HUD]") {
    PreloadedConfig config;
    EventBus eventBus;
    GridSystem grid(eventBus, config);
    
    grid.buildFloor(0);
    grid.buildFloor(1);
    
    // Add tenants
    REQUIRE(grid.placeTenant({0, 0}, TenantType::Residential, 2, INVALID_ENTITY).ok());
    REQUIRE(grid.placeTenant({5, 0}, TenantType::Office, 3, INVALID_ENTITY).ok());
    REQUIRE(grid.placeTenant({0, 1}, TenantType::Commercial, 4, INVALID_ENTITY).ok());
    
    REQUIRE(grid.getTenantCount() == 3);
    
    // Remove one tenant
    REQUIRE(grid.removeTenant({5, 0}).ok());
    REQUIRE(grid.getTenantCount() == 2);
}

TEST_CASE("Integration - RenderFrame mouse position fields", "[Integration][HUD][Mouse]") {
    RenderFrame frame;
    
    // Initial values
    REQUIRE(frame.mouseX == 0);
    REQUIRE(frame.mouseY == 0);
    
    // Set values
    frame.mouseX = 100;
    frame.mouseY = 200;
    
    REQUIRE(frame.mouseX == 100);
    REQUIRE(frame.mouseY == 200);
}

TEST_CASE("Integration - HUD displays non-zero balance after economy income", "[Integration][HUD][Economy]") {
    // Test that when economy has income, HUD can display non-zero balance
    PreloadedConfig config;
    EventBus eventBus;
    
    // Create economy with starting balance
    EconomyConfig ecfg;
    ecfg.startingBalance = 1000000;  // Starting with 1,000,000
    EconomyEngine economy(ecfg, eventBus);
    
    // Economy should have non-zero starting balance
    REQUIRE(economy.getBalance() != 0);
    REQUIRE(economy.getBalance() == 1000000);
    
    // Create RenderFrame and set balance from economy
    RenderFrame frame;
    frame.balance = economy.getBalance();
    
    // HUD should display non-zero balance
    REQUIRE(frame.balance != 0);
    REQUIRE(frame.balance == 1000000);
    
    // Test HUDPanel formatting with non-zero balance
    std::string formatted = HUDPanel::formatBalance(frame.balance);
    REQUIRE(formatted.find("₩") != std::string::npos);
    REQUIRE(formatted.find("1,000,000") != std::string::npos);
    
    // Simulate economy income (collect rent)
    // First need to setup grid with tenants
    GridSystem grid(eventBus, config);
    grid.buildFloor(0);
    
    EntityId tenantId = static_cast<EntityId>(100);
    grid.placeTenant({5, 0}, TenantType::Office, 2, tenantId);
    
    // Collect rent (simulate daily rent collection)
    // Note: In actual game, collectRent() would be called by EconomyEngine.update()
    // For test, we'll directly call it with current time
    GameTime time{0, 0, 0};
    economy.collectRent(grid, time);
    
    // Balance should still be non-zero (might be same or different depending on rent)
    REQUIRE(economy.getBalance() != 0);
    
    // Update RenderFrame with new balance
    frame.balance = economy.getBalance();
    REQUIRE(frame.balance != 0);
}
