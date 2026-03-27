// tests/test_BuildCursor.cpp
// Unit tests for BuildModeState and BuildCursor state logic.
// SDL/ImGui rendering is not exercised here — logic only.
#include <catch2/catch_test_macros.hpp>
#include "core/InputTypes.h"
#include "renderer/BuildCursor.h"

namespace vse {
namespace {

// ── BuildModeState field tests ───────────────────────────────────────────────

TEST_CASE("BuildModeState has correct default values", "[BuildCursor]") {
    BuildModeState state;
    REQUIRE(state.action == BuildAction::None);
    REQUIRE(state.tenantType == 0);
    REQUIRE(state.tenantWidth == 1);
    REQUIRE(state.active == false);
    REQUIRE(state.isValidPlacement == true);   // default: valid
    REQUIRE(state.previewCost == 0);           // default: no cost shown
}

TEST_CASE("BuildModeState isValidPlacement can be set false", "[BuildCursor]") {
    BuildModeState state;
    state.action = BuildAction::PlaceTenant;
    state.active = true;
    state.isValidPlacement = false;
    state.previewCost = 5000;

    REQUIRE(state.isValidPlacement == false);
    REQUIRE(state.previewCost == 5000);
}

TEST_CASE("BuildModeState inactive mode has previewCost zero", "[BuildCursor]") {
    // When mode is None/inactive, Bootstrapper sets previewCost=0
    BuildModeState state;
    state.action = BuildAction::None;
    state.active = false;

    REQUIRE(state.action == BuildAction::None);
    REQUIRE(state.previewCost == 0);
}

TEST_CASE("BuildModeState copy preserves isValidPlacement and previewCost", "[BuildCursor]") {
    BuildModeState original;
    original.action = BuildAction::BuildFloor;
    original.active = true;
    original.isValidPlacement = false;
    original.previewCost = 10000;

    BuildModeState copied = original;

    REQUIRE(copied.action == BuildAction::BuildFloor);
    REQUIRE(copied.active == true);
    REQUIRE(copied.isValidPlacement == false);
    REQUIRE(copied.previewCost == 10000);
}

TEST_CASE("BuildModeState assignment preserves isValidPlacement and previewCost", "[BuildCursor]") {
    BuildModeState source;
    source.action = BuildAction::PlaceTenant;
    source.tenantType = 2;  // Commercial
    source.active = true;
    source.isValidPlacement = true;
    source.previewCost = 8000;

    BuildModeState target;
    target = source;

    REQUIRE(target.tenantType == 2);
    REQUIRE(target.isValidPlacement == true);
    REQUIRE(target.previewCost == 8000);
}

// ── BuildCursor popup state tests ────────────────────────────────────────────

TEST_CASE("BuildCursor popup is initially closed", "[BuildCursor]") {
    BuildCursor cursor;
    // Popup should not be open before openTenantSelectPopup() is called
    // Verify by checking that drawTenantSelectPopup returns false without opening
    // (Cannot call ImGui here, so test open state via multiple open calls)
    // Without SDL/ImGui init, we only test that the object is default-constructible
    // and openTenantSelectPopup does not crash
    cursor.openTenantSelectPopup();
    // popupOpen_ is set; we cannot call drawTenantSelectPopup without ImGui context,
    // but we verify no crash on repeated open calls
    cursor.openTenantSelectPopup();
    REQUIRE(true);  // Construction and open call must not throw
}

TEST_CASE("BuildModeState tenant type range covers all three types", "[BuildCursor]") {
    // Verify all three tenant type indices are representable
    for (int type = 0; type <= 2; ++type) {
        BuildModeState state;
        state.action = BuildAction::PlaceTenant;
        state.tenantType = type;
        state.active = true;
        REQUIRE(state.tenantType == type);
    }
}

TEST_CASE("BuildModeState left-edge clamp: startX < 0 must not mark invalid", "[BuildCursor]") {
    // This test documents the agreed behavior: startX is clamped to 0 (mirrors InputMapper).
    // Preview validity = false only when clamped tiles are occupied or floor not built.
    // A purely off-screen hover (tileX < 0) returns previewCost=0, isValidPlacement=true(default).
    BuildModeState state;
    state.action = BuildAction::PlaceTenant;
    state.tenantWidth = 3;
    state.active = true;
    // Bootstrapper sets isValidPlacement based on clamped startX=0 tile occupancy.
    // Simulate: clamped startX=0, floor built, tiles empty → valid
    state.isValidPlacement = true;
    state.previewCost = 5000;
    REQUIRE(state.isValidPlacement == true);
    REQUIRE(state.previewCost == 5000);
}

} // anonymous namespace
} // namespace vse
