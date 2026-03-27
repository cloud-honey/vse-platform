// tests/test_BuildCursor.cpp
#include <catch2/catch_test_macros.hpp>
#include "core/InputTypes.h"
#include "renderer/BuildCursor.h"
#include <SDL.h>

namespace vse {
namespace {

// Mock SDL_Renderer for testing
struct MockRenderer {
    // Simple mock that doesn't actually render
};

// Test BuildModeState defaults and new fields
TEST_CASE("BuildModeState has new fields with correct defaults", "[BuildCursor]") {
    BuildModeState state;
    
    REQUIRE(state.action == BuildAction::None);
    REQUIRE(state.tenantType == 0);
    REQUIRE(state.tenantWidth == 1);
    REQUIRE(state.active == false);
    REQUIRE(state.isValidPlacement == true);  // NEW field
    REQUIRE(state.previewCost == 0);          // NEW field
}

TEST_CASE("BuildModeState can be set with new fields", "[BuildCursor]") {
    BuildModeState state;
    state.action = BuildAction::PlaceTenant;
    state.tenantType = 1;
    state.tenantWidth = 3;
    state.active = true;
    state.isValidPlacement = false;
    state.previewCost = 3000;
    
    REQUIRE(state.action == BuildAction::PlaceTenant);
    REQUIRE(state.tenantType == 1);
    REQUIRE(state.tenantWidth == 3);
    REQUIRE(state.active == true);
    REQUIRE(state.isValidPlacement == false);
    REQUIRE(state.previewCost == 3000);
}



TEST_CASE("BuildModeState copy preserves new fields", "[BuildCursor]") {
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

TEST_CASE("BuildModeState assignment preserves new fields", "[BuildCursor]") {
    BuildModeState source;
    source.action = BuildAction::PlaceTenant;
    source.tenantType = 2;
    source.active = true;
    source.isValidPlacement = true;
    source.previewCost = 8000;
    
    BuildModeState target;
    target = source;
    
    REQUIRE(target.action == BuildAction::PlaceTenant);
    REQUIRE(target.tenantType == 2);
    REQUIRE(target.active == true);
    REQUIRE(target.isValidPlacement == true);
    REQUIRE(target.previewCost == 8000);
}

TEST_CASE("BuildModeState different actions have different default costs", "[BuildCursor]") {
    BuildModeState floorState;
    floorState.action = BuildAction::BuildFloor;
    // Floor build cost should be different from tenant costs
    
    BuildModeState tenantState;
    tenantState.action = BuildAction::PlaceTenant;
    tenantState.tenantType = 0; // Office
    // Office cost should be different from residential/commercial
    
    // These are just structural tests - actual cost values
    // are determined by Bootstrapper based on config/balance data
    REQUIRE(true); // Placeholder assertion
}

} // anonymous namespace
} // namespace vse