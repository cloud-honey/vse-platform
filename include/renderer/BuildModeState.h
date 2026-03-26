#pragma once

namespace vse {

enum class BuildAction { None, BuildFloor, PlaceTenant };

struct BuildModeState {
    BuildAction action = BuildAction::None;
    int         tenantType = 0;   // 0=Office, 1=Residential, 2=Commercial
    int         tenantWidth = 1;  // tiles wide
    bool        active = false;
};

} // namespace vse