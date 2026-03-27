# TASK-05-005 Report: TD-001 isAnchor Refactoring

## Audit Findings

### 1. **Critical Issues Found**

#### Issue 1: GridSystem.cpp - `placeElevatorShaft()` sets `isAnchor = true` for elevator shafts
**Location:** `src/domain/GridSystem.cpp:218`
```cpp
tile.isAnchor = true;  // Each elevator shaft tile is an anchor tile
```
**Problem:** This violates the requirement that elevator shaft tiles must NEVER have `isAnchor = true`. Elevator shafts should have `isElevatorShaft = true` and `isAnchor = false`.

#### Issue 2: EconomyEngine.cpp - `payMaintenance()` incorrectly checks `isAnchor` for elevator shafts
**Location:** `src/domain/EconomyEngine.cpp:109`
```cpp
if (tileData && tileData->isAnchor) {
```
**Problem:** The code counts elevator shafts by checking if they're anchor tiles. It should count elevator shafts using the `isElevatorShaft` flag directly.

#### Issue 3: MockGridSystem in test files incorrectly sets `isAnchor = true` for elevator shafts
**Locations:** 
- `tests/test_EconomyEngine.cpp:86`: `data.isAnchor = true;  // Fixed: elevator shafts have isAnchor = true in production GridSystem`
- `tests/test_EconomyEngine.cpp:115`: `data.isAnchor = true;  // Fixed: elevator shafts have isAnchor = true in production GridSystem`

**Problem:** The mock is incorrectly mimicking the bug in production code. It should set `isAnchor = false` for elevator shafts.

### 2. **Correct Implementations Found**

#### `placeTenant()` correctly sets `isAnchor`
**Location:** `src/domain/GridSystem.cpp:108`
```cpp
tile.isAnchor = (i == 0);
```
**Status:** ✅ Correct - Only the leftmost tile (index 0) gets `isAnchor = true`.

#### `collectRent()` correctly uses `isAnchor` for tenants
**Location:** `src/domain/EconomyEngine.cpp:87`
```cpp
if (tileData.isAnchor && tileData.tenantEntity != INVALID_ENTITY) {
```
**Status:** ✅ Correct - Uses `isAnchor` to identify tenant anchor tiles.

#### `isTileEmpty()` correctly checks `isElevatorShaft`
**Location:** `src/domain/GridSystem.cpp:185`
```cpp
return tile.has_value() && tile->tenantType == TenantType::COUNT && !tile->isElevatorShaft;
```
**Status:** ✅ Correct - Uses `isElevatorShaft` to block tenant placement on shaft tiles.

#### `getTenantCount()` correctly uses `isAnchor`
**Location:** `src/domain/GridSystem.cpp:339`
```cpp
if (tile.tenantType != TenantType::COUNT && tile.isAnchor) {
```
**Status:** ✅ Correct - Counts tenants by checking `isAnchor`.

### 3. **Usage Summary**

#### `isAnchor` usage locations:
1. `src/domain/GridSystem.cpp:108` - `placeTenant()` sets `isAnchor = (i == 0)`
2. `src/domain/GridSystem.cpp:218` - `placeElevatorShaft()` sets `isAnchor = true` (❌ **BUG**)
3. `src/domain/GridSystem.cpp:247, 255` - `findAnchor()` checks `currentTile->isAnchor`
4. `src/domain/GridSystem.cpp:297` - Export serializes `isAnchor`
5. `src/domain/GridSystem.cpp:311` - Import deserializes `isAnchor`
6. `src/domain/GridSystem.cpp:339` - `getTenantCount()` checks `tile.isAnchor`
7. `src/domain/EconomyEngine.cpp:87` - `collectRent()` checks `tileData.isAnchor`
8. `src/domain/EconomyEngine.cpp:109` - `payMaintenance()` checks `tileData->isAnchor` (❌ **BUG**)
9. `src/domain/AgentSystem.cpp` (2 locations) - Checks `tile.isAnchor` for agent home/workplace

#### `isElevatorShaft` usage locations:
1. `src/domain/GridSystem.cpp:185` - `isTileEmpty()` checks `!tile->isElevatorShaft`
2. `src/domain/GridSystem.cpp:217` - `placeElevatorShaft()` sets `tile.isElevatorShaft = true`
3. `src/domain/GridSystem.cpp:234` - `isElevatorShaft()` checks `tile->isElevatorShaft`
4. `src/domain/GridSystem.cpp:298` - Export serializes `isElevatorShaft`
5. `src/domain/GridSystem.cpp:312` - Import deserializes `isElevatorShaft`
6. `src/domain/EconomyEngine.cpp:107` - `payMaintenance()` checks `grid.isElevatorShaft(coord)`
7. `src/renderer/RenderFrameCollector.cpp` - Checks `tileOpt->isElevatorShaft` for rendering

## Fixes Applied

### 1. Fixed `GridSystem::placeElevatorShaft()`
Changed line 218 in `src/domain/GridSystem.cpp`:
```cpp
tile.isAnchor = true;  // Each elevator shaft tile is an anchor tile
```
To:
```cpp
tile.isAnchor = false;  // Elevator shafts are NOT anchor tiles
```

### 2. Fixed `EconomyEngine::payMaintenance()`
Changed lines 108-110 in `src/domain/EconomyEngine.cpp`:
```cpp
// Check if this is an anchor tile (to avoid double-counting)
auto tileData = grid.getTile(coord);
if (tileData && tileData->isAnchor) {
    elevatorShaftCount++;
}
```
To:
```cpp
// Count all elevator shaft tiles
elevatorShaftCount++;
```

### 3. Fixed `MockGridSystem` in test files
Changed lines in `tests/test_EconomyEngine.cpp`:
- Line 86: `data.isAnchor = true;` → `data.isAnchor = false;`
- Line 115: `data.isAnchor = true;` → `data.isAnchor = false;`

## Tests Added

Added 4 tests to `tests/test_GridSystem.cpp`:
1. ✅ **Elevator shaft tile has `isElevatorShaft = true` and `isAnchor = false`** - Verifies elevator shafts are not anchor tiles
2. ✅ **Tenant anchor tile has `isAnchor = true` and `isElevatorShaft = false`** - Verifies tenant anchors are not elevator shafts  
3. ✅ **Non-anchor tenant tile has `isAnchor = false` and `isElevatorShaft = false`** - Verifies non-anchor tenant tiles have neither flag
4. ✅ **`isAnchor` and `isElevatorShaft` are mutually exclusive after mixed placement** - Verifies mutual exclusion across all tiles

## Build & Test Results
- **Compilation:** ✅ Success - No errors
- **Tests:** ✅ All 369 tests pass (100%)
- **Mutual Exclusion:** ✅ Enforced - `isAnchor` and `isElevatorShaft` are now mutually exclusive

## Build & Test Commands
```bash
cd /home/sykim/.openclaw/workspace/vse-platform
cd build && make -j$(nproc) 2>&1 | grep "error:" | head -10
ctest --output-on-failure 2>&1 | tail -10
```