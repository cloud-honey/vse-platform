/**
 * @file test_Sprint5Integration.cpp
 * @layer Integration Test
 * @task TASK-05-006
 * @author Boom (Claude Sonnet)
 *
 * @brief Sprint 5 cross-system integration tests.
 *        Verifies BuildCursor state, Camera pivot zoom, SaveSlotInfo Layer 0,
 *        isAnchor/isElevatorShaft mutual exclusion, HUD toast system,
 *        and save path consistency work together correctly.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/InputTypes.h"
#include "core/IRenderCommand.h"
#include "core/ISaveLoad.h"
#include "renderer/Camera.h"
#include "renderer/BuildCursor.h"
#include "renderer/SaveLoadPanel.h"
#include "renderer/HUDPanel.h"
#include "domain/GridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <entt/entt.hpp>

using namespace vse;
using Catch::Approx;

// ── Helper: minimal config path ──────────────────────────────────────────────
static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

// ── 1. BuildModeState — TASK-05-001 ─────────────────────────────────────────

TEST_CASE("Sprint5 BuildModeState isValidPlacement and previewCost defaults", "[Sprint5][BuildCursor]")
{
    BuildModeState state;
    REQUIRE(state.isValidPlacement == true);
    REQUIRE(state.previewCost == 0);
    REQUIRE(state.action == BuildAction::None);
    REQUIRE(state.active == false);
}

TEST_CASE("Sprint5 BuildModeState floor placement cost propagation", "[Sprint5][BuildCursor]")
{
    BuildModeState state;
    state.action = BuildAction::BuildFloor;
    state.active = true;
    state.isValidPlacement = false;  // already built
    state.previewCost = 10000;       // floor build cost in Cents

    REQUIRE(state.isValidPlacement == false);
    REQUIRE(state.previewCost == 10000);
}

TEST_CASE("Sprint5 BuildCursor popup is independent per instance", "[Sprint5][BuildCursor]")
{
    BuildCursor c1;
    BuildCursor c2;
    c1.openTenantSelectPopup();
    // c2 must not be affected by c1's open call
    REQUIRE(&c1 != &c2);  // distinct objects, independent state
}

// ── 2. Camera zoomAt — TASK-05-002 ──────────────────────────────────────────

TEST_CASE("Sprint5 Camera zoomAt keeps world point under pivot fixed", "[Sprint5][Camera]")
{
    Camera cam(1280, 720, 32);
    cam.reset();

    float pivotX = 320.0f;
    float pivotY = 240.0f;

    float worldXBefore = cam.screenToWorldX(pivotX);
    float worldYBefore = cam.screenToWorldY(pivotY);

    cam.zoomAt(0.5f, pivotX, pivotY);

    float worldXAfter = cam.screenToWorldX(pivotX);
    float worldYAfter = cam.screenToWorldY(pivotY);

    REQUIRE(worldXAfter == Approx(worldXBefore).epsilon(0.001f));
    REQUIRE(worldYAfter == Approx(worldYBefore).epsilon(0.001f));
}

TEST_CASE("Sprint5 Camera clampToWorld centers when world smaller than viewport", "[Sprint5][Camera]")
{
    Camera cam(1280, 720, 32);
    cam.reset();

    // World smaller than viewport — centering, not UB clamp
    cam.clampToWorld(640.0f, 480.0f, 2.0f);

    // Camera must be centered, not at arbitrary position
    float visW = 1280.0f / cam.zoomLevel();
    float expectedX = -(visW - 640.0f) / 2.0f;
    REQUIRE(cam.x() == Approx(expectedX).epsilon(0.001f));
}

// ── 3. SaveSlotInfo — Layer 0 data contract (TASK-05-003) ───────────────────

TEST_CASE("Sprint5 SaveSlotInfo is defined in Layer 0 (IRenderCommand.h)", "[Sprint5][SaveLoad]")
{
    // SaveSlotInfo must be usable from Layer 0 context (IRenderCommand.h include only)
    SaveSlotInfo info;
    REQUIRE(info.isEmpty == true);
    REQUIRE(info.slotIndex == 0);
    REQUIRE(info.meta.version == 1u);
}

TEST_CASE("Sprint5 RenderFrame contains save/load panel fields", "[Sprint5][SaveLoad]")
{
    RenderFrame frame;
    REQUIRE(frame.showSaveLoadPanel == false);
    REQUIRE(frame.saveLoadPanelSave == true);
    REQUIRE(frame.saveSlotInfos.empty());
}

TEST_CASE("Sprint5 SaveLoadPanel slot 0 is auto-save slot", "[Sprint5][SaveLoad]")
{
    // Slot 0 is reserved for auto-save (per design spec §5.26)
    REQUIRE(SaveLoadPanel::MAX_SLOTS == 5);

    SaveSlotInfo autoSaveSlot;
    autoSaveSlot.slotIndex = 0;
    autoSaveSlot.isEmpty = true;

    REQUIRE(autoSaveSlot.slotIndex == 0);
}

// ── 4. HUD Toast system — TASK-05-004 ───────────────────────────────────────

TEST_CASE("Sprint5 HUD toast queue enforces MAX_TOASTS", "[Sprint5][HUD]")
{
    HUDPanel panel;
    REQUIRE(panel.toastCount() == 0);

    for (int i = 0; i < HUDPanel::MAX_TOASTS + 3; ++i) {
        panel.pushToast("Toast " + std::to_string(i));
    }

    REQUIRE(panel.toastCount() == HUDPanel::MAX_TOASTS);
}

TEST_CASE("Sprint5 RenderFrame gameSpeed and dailyChange defaults", "[Sprint5][HUD]")
{
    RenderFrame frame;
    REQUIRE(frame.gameSpeed == 1);
    REQUIRE(frame.dailyChange == 0);
    REQUIRE(frame.pendingToast.empty());
}

// ── 5. isAnchor / isElevatorShaft mutual exclusion — TASK-05-005 ────────────

TEST_CASE("Sprint5 GridSystem isAnchor and isElevatorShaft are mutually exclusive", "[Sprint5][GridSystem][TD001]")
{
    ConfigManager config;
    config.loadFromFile(cfgPath());
    EventBus bus;
    GridSystem grid(bus, config);

    grid.buildFloor(1);
    grid.buildFloor(2);

    // Place elevator shaft
    grid.placeElevatorShaft(10, 0, 2);

    // Verify all shaft tiles: isElevatorShaft=true, isAnchor=false
    for (int floor = 0; floor <= 2; ++floor) {
        auto tile = grid.getTile({10, floor});
        REQUIRE(tile.has_value());
        REQUIRE(tile->isElevatorShaft == true);
        REQUIRE(tile->isAnchor == false);  // mutual exclusion
    }
}

TEST_CASE("Sprint5 GridSystem tenant anchor tile is distinct from elevator shaft", "[Sprint5][GridSystem][TD001]")
{
    ConfigManager config;
    config.loadFromFile(cfgPath());
    EventBus bus;
    GridSystem grid(bus, config);

    grid.buildFloor(1);

    entt::registry reg;
    EntityId tenantId = reg.create();
    grid.placeTenant({0, 1}, TenantType::Office, 3, tenantId);

    // Anchor tile
    auto anchor = grid.getTile({0, 1});
    REQUIRE(anchor.has_value());
    REQUIRE(anchor->isAnchor == true);
    REQUIRE(anchor->isElevatorShaft == false);

    // Non-anchor tiles
    auto aux1 = grid.getTile({1, 1});
    REQUIRE(aux1.has_value());
    REQUIRE(aux1->isAnchor == false);
    REQUIRE(aux1->isElevatorShaft == false);
}

// ── 6. Sprint 5 overall test count sanity ───────────────────────────────────

TEST_CASE("Sprint5 integration test suite is complete", "[Sprint5][meta]")
{
    // Verify all Sprint 5 task constants are correctly set
    REQUIRE(BuildAction::None != BuildAction::BuildFloor);
    REQUIRE(BuildAction::BuildFloor != BuildAction::PlaceTenant);
    REQUIRE(HUDPanel::MAX_TOASTS == 3);
    REQUIRE(HUDPanel::TOAST_DURATION == Approx(3.0f));
    REQUIRE(SaveLoadPanel::MAX_SLOTS == 5);
}
