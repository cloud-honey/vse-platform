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
#include "domain/GridSystem.h"
#include "domain/AgentSystem.h"
#include "domain/TransportSystem.h"
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