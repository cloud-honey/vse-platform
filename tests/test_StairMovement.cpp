/**
 * @file test_StairMovement.cpp
 * @layer Layer 1 Test
 * @task TASK-04-002
 * @author DeepSeek V3 (structure) + 붐 (completion)
 *
 * @brief NPC stair movement tests — stairs ≤4 floors, elevator >4,
 *        2 ticks per floor speed, elevator wait timeout fallback.
 */

#include "domain/AgentSystem.h"
#include "domain/GridSystem.h"
#include "domain/TransportSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include "core/SimClock.h"
#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace vse;

struct StairTestFixture {
    StairTestFixture() {
        ConfigManager cfg;
        cfg.loadFromFile(std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json");

        grid = std::make_unique<GridSystem>(bus, cfg);
        transport = std::make_unique<TransportSystem>(*grid, bus, cfg);
        agents = std::make_unique<AgentSystem>(*grid, bus, *transport);

        // Build floors 0-9
        for (int f = 0; f <= 9; ++f) grid->buildFloor(f);

        // Elevator shaft from floor 0 to 9
        grid->placeElevatorShaft(5, 0, 9);
        transport->createElevator(5, 0, 9, 8);

        // Tenants: home at floor 0, work at floor 2 (≤4 diff → stairs)
        homeId = reg.create();
        workId = reg.create();
        grid->placeTenant({0, 0}, TenantType::Residential, 2, homeId);
        grid->placeTenant({0, 2}, TenantType::Office, 3, workId);

        // Far work at floor 6 (>4 diff from floor 0 → elevator)
        farWorkId = reg.create();
        grid->placeTenant({0, 6}, TenantType::Office, 3, farWorkId);
    }

    EventBus bus;
    SimClock clock{bus};
    entt::registry reg;
    std::unique_ptr<GridSystem> grid;
    std::unique_ptr<TransportSystem> transport;
    std::unique_ptr<AgentSystem> agents;
    EntityId homeId;
    EntityId workId;
    EntityId farWorkId;
};

// Test 1: NPC uses stairs for ≤4 floor difference
TEST_CASE("StairMovement - uses stairs for <= 4 floors", "[StairMovement]") {
    StairTestFixture f;

    // Spawn NPC: home floor 0, work floor 2 (diff = 2)
    auto npcResult = f.agents->spawnAgent(f.reg, f.homeId, f.workId);
    REQUIRE(npcResult.ok());
    EntityId npc = npcResult.value;

    auto& agent = f.reg.get<AgentComponent>(npc);
    REQUIRE(agent.state == AgentState::Idle);

    // Advance to work time (hour 9)
    GameTime workTime{0, 9, 0};
    f.agents->update(f.reg, workTime);

    // Should use stairs (floor diff 2 ≤ 4)
    REQUIRE(agent.state == AgentState::UsingStairs);
    REQUIRE(agent.stairTargetFloor == 2);
    REQUIRE(agent.stairTicksRemaining == 4); // 2 floors * 2 ticks = 4
}

// Test 2: NPC uses elevator for >4 floor difference
TEST_CASE("StairMovement - uses elevator for > 4 floors", "[StairMovement]") {
    StairTestFixture f;

    // Spawn NPC: home floor 0, far work floor 6 (diff = 6)
    auto npcResult = f.agents->spawnAgent(f.reg, f.homeId, f.farWorkId);
    REQUIRE(npcResult.ok());
    EntityId npc = npcResult.value;

    auto& agent = f.reg.get<AgentComponent>(npc);

    GameTime workTime{0, 9, 0};
    f.agents->update(f.reg, workTime);

    // Should use elevator (floor diff 6 > 4)
    REQUIRE(agent.state == AgentState::WaitingElevator);
}

// Test 3: Stair movement completes in correct number of ticks
TEST_CASE("StairMovement - completes in 2 ticks per floor", "[StairMovement]") {
    StairTestFixture f;

    auto npcResult = f.agents->spawnAgent(f.reg, f.homeId, f.workId);
    REQUIRE(npcResult.ok());
    EntityId npc = npcResult.value;
    auto& agent = f.reg.get<AgentComponent>(npc);

    // Start moving (hour 9 = work time)
    GameTime t{0, 9, 0};
    f.agents->update(f.reg, t);
    REQUIRE(agent.state == AgentState::UsingStairs);
    REQUIRE(agent.stairTicksRemaining == 4); // 2 floors * 2

    // Tick 1
    f.agents->update(f.reg, t);
    REQUIRE(agent.state == AgentState::UsingStairs);
    REQUIRE(agent.stairTicksRemaining == 3);

    // Tick 2
    f.agents->update(f.reg, t);
    REQUIRE(agent.state == AgentState::UsingStairs);
    REQUIRE(agent.stairTicksRemaining == 2);

    // Tick 3
    f.agents->update(f.reg, t);
    REQUIRE(agent.state == AgentState::UsingStairs);
    REQUIRE(agent.stairTicksRemaining == 1);

    // Tick 4 — should arrive
    f.agents->update(f.reg, t);
    REQUIRE(agent.state != AgentState::UsingStairs);
}

// Test 4: NPC arrives at correct floor after stair movement
TEST_CASE("StairMovement - arrives at correct floor", "[StairMovement]") {
    StairTestFixture f;

    auto npcResult = f.agents->spawnAgent(f.reg, f.homeId, f.workId);
    REQUIRE(npcResult.ok());
    EntityId npc = npcResult.value;
    auto& agent = f.reg.get<AgentComponent>(npc);
    auto& pos = f.reg.get<PositionComponent>(npc);

    REQUIRE(pos.tile.floor == 0);

    GameTime t{0, 9, 0};

    // Trigger stairs + run 4 ticks to complete
    f.agents->update(f.reg, t); // start stairs
    f.agents->update(f.reg, t); // tick 1
    f.agents->update(f.reg, t); // tick 2
    f.agents->update(f.reg, t); // tick 3
    f.agents->update(f.reg, t); // tick 4 — arrive

    REQUIRE(pos.tile.floor == 2);
}

// Test 5: NPC transitions to Working after stair arrival during work hours
TEST_CASE("StairMovement - transitions to Working after arrival", "[StairMovement]") {
    StairTestFixture f;

    auto npcResult = f.agents->spawnAgent(f.reg, f.homeId, f.workId);
    REQUIRE(npcResult.ok());
    EntityId npc = npcResult.value;
    auto& agent = f.reg.get<AgentComponent>(npc);

    GameTime t{0, 9, 0}; // Work hour

    // Start stairs + complete
    for (int i = 0; i < 5; ++i) f.agents->update(f.reg, t);

    REQUIRE(agent.state == AgentState::Working);
}

// Test 6: Elevator wait timeout triggers stair fallback (≥5 floors, 40 tick threshold)
TEST_CASE("StairMovement - elevator wait timeout triggers stair fallback", "[StairMovement]") {
    StairTestFixture f;

    // NPC: floor 0 → floor 6 (diff=6 > 4 → elevator initially)
    auto npcResult = f.agents->spawnAgent(f.reg, f.homeId, f.farWorkId);
    REQUIRE(npcResult.ok());
    EntityId npc = npcResult.value;
    auto& agent = f.reg.get<AgentComponent>(npc);

    // Keep satisfaction high to prevent Leaving during wait
    agent.satisfaction = 100.0f;

    GameTime t{0, 9, 0};
    f.agents->update(f.reg, t); // Enter WaitingElevator
    REQUIRE(agent.state == AgentState::WaitingElevator);

    // Tick 41 times — threshold for ≥5 floors is 40 ticks
    for (int i = 0; i < 41; ++i) {
        agent.satisfaction = 100.0f; // Prevent satisfaction decay from triggering Leaving
        f.agents->update(f.reg, t);
        if (agent.state == AgentState::UsingStairs) break;
    }

    REQUIRE(agent.state == AgentState::UsingStairs);
    REQUIRE(agent.stairTargetFloor == 6);
    REQUIRE(agent.stairTicksRemaining == 12); // 6 floors * 2 ticks
}

// Test 7: NPC still waiting before timeout threshold
TEST_CASE("StairMovement - no stair fallback before timeout", "[StairMovement]") {
    StairTestFixture f;

    auto npcResult = f.agents->spawnAgent(f.reg, f.homeId, f.farWorkId);
    REQUIRE(npcResult.ok());
    EntityId npc = npcResult.value;
    auto& agent = f.reg.get<AgentComponent>(npc);

    GameTime t{0, 9, 0};
    f.agents->update(f.reg, t); // Enter WaitingElevator
    REQUIRE(agent.state == AgentState::WaitingElevator);

    // Only 20 ticks — below 40 threshold for ≥5 floors
    for (int i = 0; i < 20; ++i) {
        f.agents->update(f.reg, t);
        // May board elevator if available — that's fine too
    }

    // Should NOT have switched to stairs yet (threshold is 40)
    REQUIRE(agent.state != AgentState::UsingStairs);
}
