#pragma once
#include "core/IGridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <nlohmann/json.hpp>
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

    // ── SaveLoad support ───────────────────────────────────────────────────
    /** Export all floor/tile state to JSON (for MessagePack serialization) */
    nlohmann::json exportState() const;
    /** Import floor/tile state from JSON. Clears existing floors first. */
    void importState(const nlohmann::json& j);

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

    // Internal helper — not part of IGridSystem public contract
    // Walks left from anyTile to find the anchor tile of a multi-tile tenant.
    std::optional<TileCoord> findAnchor(TileCoord anyTile) const;
};

} // namespace vse
