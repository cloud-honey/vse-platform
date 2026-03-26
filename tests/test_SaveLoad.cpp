#include "domain/SaveLoadSystem.h"
#include "domain/GridSystem.h"
#include "domain/AgentSystem.h"
#include "domain/TransportSystem.h"
#include "domain/EconomyEngine.h"
#include "domain/StarRatingSystem.h"
#include "core/SimClock.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <entt/entt.hpp>
#include <filesystem>
#include <fstream>

using namespace vse;
namespace fs = std::filesystem;

static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

static std::string balancePath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/data/balance.json";
}

struct PreloadedConfig : ConfigManager {
    PreloadedConfig() { loadFromFile(cfgPath()); }
};

static EconomyConfig loadEconomyConfig() {
    EconomyConfig ec;
    ec.startingBalance             = 200000;
    ec.officeRentPerTilePerDay     = 100;
    ec.residentialRentPerTilePerDay = 50;
    ec.commercialRentPerTilePerDay = 200;
    ec.elevatorMaintenancePerDay   = 500;
    return ec;
}

// ── Full game state fixture ─────────────────────────────────────────────────
struct SaveLoadFixture {
    EventBus           bus;
    PreloadedConfig    cfg;
    GridSystem         grid;
    TransportSystem    transport;
    AgentSystem        agents;
    EconomyEngine      economy;
    StarRatingSystem   starRating;
    SimClock           clock;
    entt::registry     reg;
    SaveLoadSystem     saveLoad;

    std::string testSaveDir;
    std::string testSaveFile;

    // Tenant entity IDs (manually created, placed in grid)
    EntityId homeTenantId  = INVALID_ENTITY;
    EntityId workTenantId  = INVALID_ENTITY;
    EntityId elevId        = INVALID_ENTITY;

    SaveLoadFixture()
        : grid(bus, cfg)
        , transport(grid, bus, cfg)
        , agents(grid, bus, transport)
        , economy(loadEconomyConfig())
        , starRating(bus, StarRatingSystem::Config{})
        , clock(bus)
        , saveLoad(reg, clock, grid, economy, starRating, transport, agents, testDir())
        , testSaveDir(testDir())
        , testSaveFile(testDir() + "/test_save.vsesave")
    {
        // Build floors 0, 1, 2
        for (int f = 1; f <= 6; ++f) grid.buildFloor(f);

        // Elevator shaft x=0, floors 0~2
        grid.placeElevatorShaft(0, 0, 6);
        auto elevResult = transport.createElevator(0, 0, 6, 8);
        REQUIRE(elevResult.ok());
        elevId = elevResult.value;

        // Home tenant floor 0, x=2
        homeTenantId = reg.create();
        grid.placeTenant({2, 0}, TenantType::Residential, 2, homeTenantId);

        // Work tenant floor 2, x=2
        workTenantId = reg.create();
        grid.placeTenant({2, 6}, TenantType::Office, 2, workTenantId);

        // Star rating singleton
        starRating.initRegistry(reg);
    }

    ~SaveLoadFixture() {
        // Cleanup test save files
        if (fs::exists(testSaveDir)) {
            fs::remove_all(testSaveDir);
        }
    }

    static std::string testDir() {
        return std::string(VSE_PROJECT_ROOT) + "/test_saves_tmp";
    }
};

// ── Test 1: Basic save and load round-trip ──────────────────────────────────

TEST_CASE("SaveLoad - basic save and load round-trip", "[SaveLoad]") {
    SaveLoadFixture f;

    // Spawn an agent
    auto spawnResult = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(spawnResult.ok());
    EntityId agentId = spawnResult.value;

    // Advance clock
    for (int i = 0; i < 10; ++i) f.clock.advanceTick();

    // Save
    auto saveResult = f.saveLoad.save(f.testSaveFile);
    REQUIRE(saveResult.ok());
    REQUIRE(fs::exists(f.testSaveFile));

    // Load into same system (after clearing)
    auto loadResult = f.saveLoad.load(f.testSaveFile);
    REQUIRE(loadResult.ok());

    // Verify agent count restored
    REQUIRE(f.agents.activeAgentCount() == 1);
}

// ── Test 2: SimClock state preserved ────────────────────────────────────────

TEST_CASE("SaveLoad - SimClock state preserved", "[SaveLoad]") {
    SaveLoadFixture f;

    // Advance clock to specific state
    for (int i = 0; i < 100; ++i) f.clock.advanceTick();
    f.clock.setSpeed(2);
    f.clock.pause();

    SimTick savedTick   = f.clock.currentTick();
    int     savedSpeed  = f.clock.speed();
    bool    savedPaused = f.clock.isPaused();

    f.saveLoad.save(f.testSaveFile);
    f.saveLoad.load(f.testSaveFile);

    REQUIRE(f.clock.currentTick()   == savedTick);
    REQUIRE(f.clock.speed()  == savedSpeed);
    REQUIRE(f.clock.isPaused()  == savedPaused);
}

// ── Test 3: Grid floors and tiles preserved ─────────────────────────────────

TEST_CASE("SaveLoad - Grid floors and tiles preserved", "[SaveLoad]") {
    SaveLoadFixture f;

    // Save state
    int floorsBefore = f.grid.builtFloorCount();
    auto tile00 = f.grid.getTile({0, 0});
    auto tile20 = f.grid.getTile({2, 0});

    f.saveLoad.save(f.testSaveFile);
    f.saveLoad.load(f.testSaveFile);

    REQUIRE(f.grid.builtFloorCount() == floorsBefore);
    REQUIRE(f.grid.isFloorBuilt(0));
    REQUIRE(f.grid.isFloorBuilt(1));
    REQUIRE(f.grid.isFloorBuilt(2));

    // Elevator shaft preserved
    REQUIRE(f.grid.isElevatorShaft({0, 0}));
    REQUIRE(f.grid.isElevatorShaft({0, 1}));
    REQUIRE(f.grid.isElevatorShaft({0, 2}));

    // Tenant tile preserved
    auto restoredTile = f.grid.getTile({2, 0});
    REQUIRE(restoredTile.has_value());
    REQUIRE(restoredTile->tenantType == TenantType::Residential);
    REQUIRE(restoredTile->isAnchor == true);
    REQUIRE(restoredTile->tileWidth == 2);
}

// ── Test 4: Economy balance preserved ───────────────────────────────────────

TEST_CASE("SaveLoad - Economy balance preserved", "[SaveLoad]") {
    SaveLoadFixture f;

    f.economy.addIncome(f.homeTenantId, TenantType::Residential, 5000, {0, 9, 0});
    f.economy.addExpense("maintenance", 1000, {0, 9, 0});

    int64_t balBefore  = f.economy.getBalance();
    int64_t incBefore  = f.economy.getDailyIncome();
    int64_t expBefore  = f.economy.getDailyExpense();

    f.saveLoad.save(f.testSaveFile);
    f.saveLoad.load(f.testSaveFile);

    REQUIRE(f.economy.getBalance()      == balBefore);
    REQUIRE(f.economy.getDailyIncome()  == incBefore);
    REQUIRE(f.economy.getDailyExpense() == expBefore);
}

// ── Test 5: Agent components preserved ──────────────────────────────────────

TEST_CASE("SaveLoad - Agent components preserved (state, satisfaction, position)", "[SaveLoad]") {
    SaveLoadFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok());
    EntityId agentId = result.value;

    // Modify agent state
    auto& agent = f.reg.get<AgentComponent>(agentId);
    agent.satisfaction = 75.5f;
    agent.stress       = 42.0f;   // TASK-03-002: set non-default stress
    agent.state        = AgentState::Working;

    auto& pos = f.reg.get<PositionComponent>(agentId);
    pos.tile = {5, 2};
    pos.pixel = {160.0f, 64.0f};

    float savedSat   = agent.satisfaction;
    float savedStress = agent.stress;
    int   savedState  = static_cast<int>(agent.state);

    f.saveLoad.save(f.testSaveFile);
    f.saveLoad.load(f.testSaveFile);

    // Find the restored agent
    auto agentView = f.reg.view<AgentComponent, PositionComponent>();
    int viewCount = 0; for (auto _ : agentView) { (void)_; ++viewCount; } REQUIRE(viewCount >= 1);

    bool found = false;
    for (auto e : agentView) {
        auto& a = agentView.get<AgentComponent>(e);
        auto& p = agentView.get<PositionComponent>(e);
        if (static_cast<int>(a.state) == savedState) {
            REQUIRE_THAT(a.satisfaction, Catch::Matchers::WithinAbs(savedSat, 0.01));
            REQUIRE_THAT(a.stress,       Catch::Matchers::WithinAbs(savedStress, 0.01));  // TASK-03-002
            REQUIRE(p.tile.x == 5);
            REQUIRE(p.tile.floor == 2);
            REQUIRE_THAT(p.pixel.x, Catch::Matchers::WithinAbs(160.0f, 0.01));
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

// ── Test 6: Entity cross-references survive round-trip ──────────────────────

TEST_CASE("SaveLoad - homeTenant/workplaceTenant references survive round-trip", "[SaveLoad]") {
    SaveLoadFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok());

    f.saveLoad.save(f.testSaveFile);
    f.saveLoad.load(f.testSaveFile);

    // Verify the agent's homeTenant and workplaceTenant point to valid entities
    // that match grid tile entities
    auto agentView = f.reg.view<AgentComponent>();
    int viewCount = 0; for (auto _ : agentView) { (void)_; ++viewCount; } REQUIRE(viewCount >= 1);

    for (auto e : agentView) {
        auto& a = agentView.get<AgentComponent>(e);
        // Grid tiles should reference the same tenant entities
        auto homeTile = f.grid.getTile({2, 0});
        auto workTile = f.grid.getTile({2, 6});
        REQUIRE(homeTile.has_value());
        REQUIRE(workTile.has_value());
        REQUIRE(homeTile->tenantEntity == a.homeTenant);
        REQUIRE(workTile->tenantEntity == a.workplaceTenant);
    }
}

// ── Test 7: Metadata can be read without full load ──────────────────────────

TEST_CASE("SaveLoad - readMetadata without full load", "[SaveLoad]") {
    SaveLoadFixture f;

    for (int i = 0; i < 600; ++i) f.clock.advanceTick(); // 1 hour

    f.saveLoad.save(f.testSaveFile);

    auto metaResult = f.saveLoad.readMetadata(f.testSaveFile);
    REQUIRE(metaResult.ok());

    auto& meta = metaResult.value;
    REQUIRE(meta.version == 1);
    REQUIRE(meta.balance == f.economy.getBalance());
}

// ── Test 8: quickSave and quickLoad ─────────────────────────────────────────

TEST_CASE("SaveLoad - quickSave and quickLoad", "[SaveLoad]") {
    SaveLoadFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok());

    REQUIRE(f.saveLoad.quickSave().ok());
    REQUIRE(f.saveLoad.quickLoad().ok());

    REQUIRE(f.agents.activeAgentCount() == 1);
}

// ── Test 9: Multiple agents saved and restored ──────────────────────────────

TEST_CASE("SaveLoad - multiple agents saved and restored", "[SaveLoad]") {
    SaveLoadFixture f;

    // Create second home on floor 0
    EntityId home2 = f.reg.create();
    f.grid.placeTenant({6, 0}, TenantType::Residential, 2, home2);

    auto r1 = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    auto r2 = f.agents.spawnAgent(f.reg, home2, f.workTenantId);
    REQUIRE(r1.ok());
    REQUIRE(r2.ok());

    REQUIRE(f.agents.activeAgentCount() == 2);

    f.saveLoad.save(f.testSaveFile);
    f.saveLoad.load(f.testSaveFile);

    REQUIRE(f.agents.activeAgentCount() == 2);
}

// ── Test 10: Transport state preserved ──────────────────────────────────────

TEST_CASE("SaveLoad - Transport elevator state preserved", "[SaveLoad]") {
    SaveLoadFixture f;

    // Move elevator to floor 1
    f.transport.callElevator(0, 1, ElevatorDirection::Up);
    for (int i = 0; i < 10; ++i) f.transport.update({0, 9, 0});

    auto allElevsBefore = f.transport.getAllElevators();
    REQUIRE(allElevsBefore.size() == 1);

    f.saveLoad.save(f.testSaveFile);
    f.saveLoad.load(f.testSaveFile);

    auto allElevsAfter = f.transport.getAllElevators();
    REQUIRE(allElevsAfter.size() == 1);
}

// ── Test 11: Load non-existent file returns failure ─────────────────────────

TEST_CASE("SaveLoad - load non-existent file returns failure", "[SaveLoad]") {
    SaveLoadFixture f;

    auto result = f.saveLoad.load("/tmp/nonexistent_12345.vsesave");
    REQUIRE_FALSE(result.ok());
}

// ── Test 12: listSaves returns .vsesave files ───────────────────────────────

TEST_CASE("SaveLoad - listSaves returns saved files", "[SaveLoad]") {
    SaveLoadFixture f;

    f.saveLoad.save(f.testSaveFile);

    auto saves = f.saveLoad.listSaves();
    REQUIRE(saves.size() >= 1);
    
    bool found = false;
    for (const auto& s : saves) {
        if (s.find("test_save.vsesave") != std::string::npos) {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

// ── Test 13: Version mismatch rejects load ──────────────────────────────────

TEST_CASE("SaveLoad - version mismatch rejects load", "[SaveLoad]") {
    SaveLoadFixture f;

    // Create a save with wrong version
    nlohmann::json fakeRoot;
    fakeRoot["metadata"] = {{"version", 999}};
    auto msgpack = nlohmann::json::to_msgpack(fakeRoot);

    std::string fakePath = f.testSaveDir + "/fake.vsesave";
    std::ofstream out(fakePath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(msgpack.data()),
              static_cast<std::streamsize>(msgpack.size()));
    out.close();

    auto result = f.saveLoad.load(fakePath);
    REQUIRE_FALSE(result.ok());
}

// ── Test 14: Elevator passenger EntityId remapped after load ────────────────

TEST_CASE("SaveLoad - elevator passenger EntityId remapped after load", "[SaveLoad]") {
    SaveLoadFixture f;

    // Spawn agent, transition to WaitingElevator, board
    auto spawnResult = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(spawnResult.ok());
    EntityId agentId = spawnResult.value;

    // 9am → WaitingElevator
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // Drive elevator to floor 0 Boarding, then board agent
    f.transport.update(GameTime{0, 9, 0}); // DoorOpening (already at floor 0)
    f.transport.update(GameTime{0, 9, 0}); // Boarding
    f.agents.update(f.reg, GameTime{0, 9, 0}); // Board
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);

    // Save with agent on elevator
    f.saveLoad.save(f.testSaveFile);
    f.saveLoad.load(f.testSaveFile);

    // Find the restored agent
    auto agentView = f.reg.view<AgentComponent>();
    EntityId restoredAgent = INVALID_ENTITY;
    for (auto e : agentView) {
        restoredAgent = e;
        break;
    }
    REQUIRE(restoredAgent != INVALID_ENTITY);

    // Verify transport has the correct (remapped) passenger ID
    auto allElevs = f.transport.getAllElevators();
    REQUIRE(allElevs.size() == 1);
    auto snap = f.transport.getElevatorState(allElevs[0]);
    REQUIRE(snap.has_value());
    // Passenger list should contain the remapped agent entity
    bool foundPassenger = false;
    for (auto p : snap->passengers) {
        if (p == restoredAgent) {
            foundPassenger = true;
            break;
        }
    }
    REQUIRE(foundPassenger);
}
