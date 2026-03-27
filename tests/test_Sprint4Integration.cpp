/**
 * @file test_Sprint4Integration.cpp
 * @layer Integration Test
 * @task TASK-04-007
 * @author Boom (Claude Opus)
 *
 * @brief Sprint 4 cross-system integration tests.
 *        Verifies GameOverSystem, GameStateManager, TenantSystem,
 *        EconomyEngine settlement, and stair movement work together.
 *
 * @note Cross-validation review applied (GPT/Gemini/DeepSeek 2026-03-27):
 *       - Fixed mass exodus interference in GameOver/Victory tests
 *       - Added positive victory path test
 *       - Strengthened rent collection assertions
 *       - Zero-initialized EconomyConfig structs
 *       - Added mass exodus integration test
 */

#include <catch2/catch_test_macros.hpp>
#include "core/SimClock.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include "core/ContentRegistry.h"
#include "core/GameStateManager.h"
#include "domain/GridSystem.h"
#include "domain/AgentSystem.h"
#include "domain/TransportSystem.h"
#include "domain/EconomyEngine.h"
#include "domain/StarRatingSystem.h"
#include "domain/GameOverSystem.h"
#include "domain/TenantSystem.h"
#include <entt/entt.hpp>

using namespace vse;

static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

struct PreloadedConfig : ConfigManager {
    PreloadedConfig() { loadFromFile(cfgPath()); }
};

// ── GameStateManager + GameOverSystem integration (mass exodus path) ──

TEST_CASE("Integration S4 - Mass exodus GameOver transitions state machine",
          "[Integration][Sprint4][GameOver][MassExodus][StateMachine]") {
    EventBus eventBus;
    GameStateManager stateManager(eventBus);
    PreloadedConfig config;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);
    GameOverSystem gameOver(grid, agents, eventBus);

    // Start game: MainMenu → Playing
    REQUIRE(stateManager.transition(GameState::Playing));
    REQUIRE(stateManager.getState() == GameState::Playing);

    // Subscribe to GameOver to transition state
    eventBus.subscribe(EventType::GameOver, [&](const Event& e) {
        auto payload = std::any_cast<GameOverPayload>(e.payload);
        REQUIRE(payload.reason == "mass_exodus");
        stateManager.transition(GameState::GameOver);
    });

    // Lobby auto-built (1 floor), 0 NPCs → mass exodus after 7 days
    // capacity = 1 * 10 = 10, threshold = 1, activeNPCs = 0 < 1
    for (int day = 0; day < 8; ++day) {
        GameTime t{day, 0, 0};
        gameOver.update(t, -100, 0);
        eventBus.flush();
    }

    REQUIRE(gameOver.isGameOver());
    REQUIRE(stateManager.getState() == GameState::GameOver);

    // From GameOver, can go back to MainMenu
    REQUIRE(stateManager.transition(GameState::MainMenu));
    REQUIRE(stateManager.getState() == GameState::MainMenu);
}

// ── Victory positive path: manually fire TowerAchieved event ──

TEST_CASE("Integration S4 - Victory event transitions state to Victory",
          "[Integration][Sprint4][Victory][StateMachine]") {
    EventBus eventBus;
    GameStateManager stateManager(eventBus);

    stateManager.transition(GameState::Playing);
    REQUIRE(stateManager.getState() == GameState::Playing);

    // GameStateManager does not auto-subscribe to TowerAchieved —
    // the game loop (Bootstrapper) wires the transition externally.
    // Simulate that wiring here.
    eventBus.subscribe(EventType::TowerAchieved, [&](const Event&) {
        stateManager.transition(GameState::Victory);
    });

    // Manually publish victory event
    TowerAchievedPayload payload;
    payload.day = 90;
    payload.starRating = 5;
    payload.floorCount = 100;
    payload.npcCount = 300;

    Event ev;
    ev.type = EventType::TowerAchieved;
    ev.payload = payload;
    eventBus.publish(ev);
    eventBus.flush();

    REQUIRE(stateManager.getState() == GameState::Victory);

    // From Victory, can go back to MainMenu
    REQUIRE(stateManager.transition(GameState::MainMenu));
    REQUIRE(stateManager.getState() == GameState::MainMenu);
}

// ── Victory negative path: conditions not met ──

TEST_CASE("Integration S4 - Victory does NOT fire with insufficient NPCs",
          "[Integration][Sprint4][Victory]") {
    PreloadedConfig config;
    EventBus eventBus;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);
    GameOverSystem gameOver(grid, agents, eventBus);

    // Place NPC to prevent mass exodus from interfering
    entt::registry reg;
    EntityId homeId = reg.create();
    grid.placeTenant({2, 0}, TenantType::Residential, 2, homeId);
    EntityId workId = reg.create();
    grid.placeTenant({6, 0}, TenantType::Office, 2, workId);
    auto spawnResult = agents.spawnAgent(reg, homeId, workId);
    REQUIRE(spawnResult.ok());

    // 5-star, high balance, but only 1 NPC (need 300)
    for (int day = 0; day < 91; ++day) {
        GameTime t{day, 0, 0};
        gameOver.update(t, 1000000, 5);
        eventBus.flush();
    }
    REQUIRE_FALSE(gameOver.isVictoryAchieved());
    REQUIRE_FALSE(gameOver.isGameOver()); // no mass exodus either
}

// ── TenantSystem + EconomyEngine daily settlement ──

TEST_CASE("Integration S4 - TenantSystem collects rent via EconomyEngine on day change",
          "[Integration][Sprint4][Tenant][Economy]") {
    PreloadedConfig config;
    EventBus eventBus;
    entt::registry reg;
    GridSystem grid(eventBus, config);

    ContentRegistry content;
    auto loadResult = content.loadContentPack(
        std::string(VSE_PROJECT_ROOT) + "/assets");
    REQUIRE(loadResult.ok());

    EconomyConfig ecfg{};
    ecfg.startingBalance = 1000000;
    ecfg.officeRentPerTilePerDay = 500;
    ecfg.residentialRentPerTilePerDay = 300;
    ecfg.commercialRentPerTilePerDay = 800;
    ecfg.elevatorMaintenancePerDay = 1000;
    EconomyEngine economy(ecfg, eventBus);

    TenantSystem tenants(grid, eventBus, economy);

    grid.buildFloor(0);

    // Place an office tenant via TenantSystem
    auto officeResult = tenants.placeTenant(reg, TenantType::Office, {0, 0}, content);
    REQUIRE(officeResult.ok());

    int64_t balanceBefore = economy.getBalance();

    // Track rent income via event
    int64_t rentCollected = 0;
    eventBus.subscribe(EventType::IncomeReceived, [&](const Event& e) {
        auto payload = std::any_cast<IncomeRecord>(e.payload);
        rentCollected += payload.amount;
    });

    // Simulate day change → rent collection
    GameTime day1{1, 0, 0};
    tenants.onDayChanged(reg, day1);
    eventBus.flush();

    int64_t balanceAfter = economy.getBalance();
    // Balance should have changed (rent - maintenance)
    REQUIRE(balanceAfter != balanceBefore);
    // Net should be positive: office rent (500 * 4 = 2000) > maintenance (100 * 4 = 400)
    REQUIRE(balanceAfter > balanceBefore);
}

// ── EconomyEngine periodic settlement (quarterly tax) ──

TEST_CASE("Integration S4 - Quarterly tax deducts from balance",
          "[Integration][Sprint4][Economy][Settlement]") {
    EventBus eventBus;

    EconomyConfig ecfg{};
    ecfg.startingBalance = 1000000;
    ecfg.officeRentPerTilePerDay = 500;
    ecfg.residentialRentPerTilePerDay = 300;
    ecfg.commercialRentPerTilePerDay = 800;
    ecfg.elevatorMaintenancePerDay = 1000;
    EconomyEngine economy(ecfg, eventBus);

    int64_t initialBalance = economy.getBalance();
    REQUIRE(initialBalance == 1000000);

    // Advance through 90 days (1 quarter) to trigger quarterly settlement
    // economy.update fires settlement at day change (hour=0, minute=0)
    bool taxDeducted = false;
    int64_t taxAmount = 0;
    eventBus.subscribe(EventType::QuarterlySettlement, [&](const Event& e) {
        auto payload = std::any_cast<QuarterlySettlementPayload>(e.payload);
        taxDeducted = true;
        taxAmount = payload.taxAmount;
    });

    for (int day = 1; day <= 90; ++day) {
        GameTime t{day, 0, 0}; // midnight of each day
        economy.update(t);
        eventBus.flush();
        if (taxDeducted) break;
    }

    REQUIRE(taxDeducted);
    // Tax should be 5% of balance
    REQUIRE(taxAmount == static_cast<int64_t>(initialBalance * 0.05));
    REQUIRE(economy.getBalance() < initialBalance);
}

// ── Stair movement integration (NPC uses stairs for ≤4 floor difference) ──

TEST_CASE("Integration S4 - NPC uses stairs for short vertical travel",
          "[Integration][Sprint4][Stairs][Agent]") {
    PreloadedConfig config;
    EventBus eventBus;
    entt::registry reg;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);

    // Build floors 0 and 2 (2-floor difference → stairs)
    grid.buildFloor(0);
    grid.buildFloor(2);

    EntityId homeId = reg.create();
    grid.placeTenant({2, 0}, TenantType::Residential, 2, homeId);
    EntityId workId = reg.create();
    grid.placeTenant({2, 2}, TenantType::Office, 2, workId);

    auto spawnResult = agents.spawnAgent(reg, homeId, workId);
    REQUIRE(spawnResult.ok());
    EntityId agentId = spawnResult.value;

    // Verify initial position is on floor 0
    auto* pos = reg.try_get<PositionComponent>(agentId);
    REQUIRE(pos != nullptr);
    REQUIRE(pos->tile.floor == 0);

    // Run simulation through work start (tick 540 = 9 AM)
    bool sawUsingStairs = false;
    for (int tick = 0; tick < 600; ++tick) {
        eventBus.flush();
        GameTime t = GameTime::fromTick(tick);
        agents.update(reg, t);

        auto state = agents.getState(reg, agentId);
        if (state == AgentState::UsingStairs) {
            sawUsingStairs = true;
        }
    }

    // NPC should have used stairs at some point for 2-floor travel
    REQUIRE(sawUsingStairs);
}

// ── GameStateManager pause/resume during gameplay ──

TEST_CASE("Integration S4 - Pause and resume game state",
          "[Integration][Sprint4][StateMachine]") {
    EventBus eventBus;
    GameStateManager stateManager(eventBus);

    // Initial state
    REQUIRE(stateManager.getState() == GameState::MainMenu);

    // Start game
    REQUIRE(stateManager.transition(GameState::Playing));

    // Pause
    REQUIRE(stateManager.transition(GameState::Paused));
    REQUIRE(stateManager.getState() == GameState::Paused);

    // Resume
    REQUIRE(stateManager.transition(GameState::Playing));
    REQUIRE(stateManager.getState() == GameState::Playing);

    // Pause again → quit to menu
    REQUIRE(stateManager.transition(GameState::Paused));
    REQUIRE(stateManager.transition(GameState::MainMenu));
    REQUIRE(stateManager.getState() == GameState::MainMenu);
}

// ── Bankruptcy game over (isolated from mass exodus) ──

TEST_CASE("Integration S4 - Bankruptcy triggers after sustained negative balance",
          "[Integration][Sprint4][Economy][GameOver]") {
    PreloadedConfig config;
    EventBus eventBus;
    entt::registry reg;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);
    GameOverSystem gameOver(grid, agents, eventBus);

    // Place residential + office on floor 0, spawn NPC to prevent mass exodus
    // Lobby auto-built: capacity = 10, threshold = 1, need ≥ 1 NPC
    EntityId homeId = reg.create();
    grid.placeTenant({2, 0}, TenantType::Residential, 2, homeId);
    EntityId workId = reg.create();
    grid.placeTenant({6, 0}, TenantType::Office, 2, workId);
    auto spawnResult = agents.spawnAgent(reg, homeId, workId);
    REQUIRE(spawnResult.ok());

    std::string gameOverReason;
    eventBus.subscribe(EventType::GameOver, [&](const Event& e) {
        auto payload = std::any_cast<GameOverPayload>(e.payload);
        gameOverReason = payload.reason;
    });

    // Simulate 30 days with negative balance
    for (int day = 0; day < 31; ++day) {
        GameTime t{day, 0, 0};
        gameOver.update(t, -100, 0);
        eventBus.flush();
    }

    REQUIRE(gameOver.isGameOver());
    REQUIRE(gameOverReason == "bankruptcy");
}

// ── Mass Exodus game over (isolated test) ──

TEST_CASE("Integration S4 - Mass exodus triggers with zero NPCs",
          "[Integration][Sprint4][MassExodus][GameOver]") {
    PreloadedConfig config;
    EventBus eventBus;
    GridSystem grid(eventBus, config);
    AgentSystem agents(grid, eventBus);
    GameOverSystem gameOver(grid, agents, eventBus);

    // Lobby auto-built: capacity = 10, threshold = 1, 0 NPCs < 1
    std::string gameOverReason;
    eventBus.subscribe(EventType::GameOver, [&](const Event& e) {
        auto payload = std::any_cast<GameOverPayload>(e.payload);
        gameOverReason = payload.reason;
    });

    // Mass exodus fires after 7 consecutive days
    for (int day = 0; day < 8; ++day) {
        GameTime t{day, 0, 0};
        gameOver.update(t, 1000000, 0); // positive balance, no bankruptcy
        eventBus.flush();
    }

    REQUIRE(gameOver.isGameOver());
    REQUIRE(gameOverReason == "mass_exodus");
}
