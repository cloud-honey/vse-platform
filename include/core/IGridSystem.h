#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <optional>
#include <vector>

namespace vse {

struct TileData {
    TenantType tenantType = TenantType::COUNT;  // COUNT = empty
    EntityId tenantEntity = INVALID_ENTITY;
    bool isAnchor = false;
    bool isLobby = false;
    bool isElevatorShaft = false;
    int tileWidth = 1;              // Only meaningful on anchor tile
};

class IGridSystem {
public:
    virtual ~IGridSystem() = default;

    virtual int maxFloors() const = 0;
    virtual int floorWidth() const = 0;

    virtual Result<bool> buildFloor(int floor) = 0;
    virtual bool isFloorBuilt(int floor) const = 0;
    virtual int builtFloorCount() const = 0;

    virtual Result<bool> placeTenant(TileCoord anchor, TenantType type, int width, EntityId entity) = 0;
    virtual Result<bool> removeTenant(TileCoord anyTile) = 0;
    virtual std::optional<TileData> getTile(TileCoord pos) const = 0;
    virtual bool isTileEmpty(TileCoord pos) const = 0;
    virtual bool isValidCoord(TileCoord pos) const = 0;

    virtual std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const = 0;

    virtual Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) = 0;
    virtual bool isElevatorShaft(TileCoord pos) const = 0;

    virtual std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const = 0;
    virtual std::optional<TileCoord> findAnchor(TileCoord anyTile) const = 0;
};

} // namespace vse