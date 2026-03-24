#pragma once
#include "core/IGridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <unordered_map>

namespace vse {

class GridSystem : public IGridSystem {
public:
    GridSystem(EventBus& bus, const ConfigManager& config);

    int maxFloors() const override;
    int floorWidth() const override;
    Result<bool> buildFloor(int floor) override;
    bool isFloorBuilt(int floor) const override;
    int builtFloorCount() const override;
    Result<bool> placeTenant(TileCoord anchor, TenantType type, int width, EntityId entity) override;
    Result<bool> removeTenant(TileCoord anyTile) override;
    std::optional<TileData> getTile(TileCoord pos) const override;
    bool isTileEmpty(TileCoord pos) const override;
    bool isValidCoord(TileCoord pos) const override;
    std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const override;
    Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) override;
    bool isElevatorShaft(TileCoord pos) const override;
    std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const override;
    std::optional<TileCoord> findAnchor(TileCoord anyTile) const override;

private:
    EventBus& eventBus_;
    int maxFloors_;
    int floorWidth_;

    struct FloorData {
        std::vector<TileData> tiles;
        bool built = false;
    };
    std::unordered_map<int, FloorData> floors_;

    size_t tileIndex(int x) const;
};

} // namespace vse