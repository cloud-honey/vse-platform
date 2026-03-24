#include "domain/GridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <algorithm>
#include <cmath>

namespace vse {

GridSystem::GridSystem(EventBus& bus, const ConfigManager& config)
    : eventBus_(bus)
    , maxFloors_(config.getInt("simulation.maxFloors", 10))
    , floorWidth_(config.getInt("grid.tilesPerFloor", 20))
{
    // 층 0은 게임 시작 시 자동 건설 (로비)
    // floor 0 전 타일을 isLobby=true로 초기화 — NPC 스폰/디스폰 기준점
    FloorData floor0;
    floor0.tiles.resize(floorWidth_);
    for (auto& tile : floor0.tiles) {
        tile.isLobby = true;
    }
    floor0.built = true;
    floors_[0] = floor0;
}

int GridSystem::maxFloors() const {
    return maxFloors_;
}

int GridSystem::floorWidth() const {
    return floorWidth_;
}

Result<bool> GridSystem::buildFloor(int floor) {
    if (floor < 0 || floor >= maxFloors_) {
        return Result<bool>::failure(ErrorCode::OutOfRange);
    }
    
    auto it = floors_.find(floor);
    if (it != floors_.end() && it->second.built) {
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }
    
    FloorData floorData;
    floorData.tiles.resize(floorWidth_);
    floorData.built = true;
    floors_[floor] = floorData;
    
    // 이벤트 발행
    Event event;
    event.type = EventType::FloorBuilt;
    event.payload = floor;
    eventBus_.publish(event);
    
    return Result<bool>::success(true);
}

bool GridSystem::isFloorBuilt(int floor) const {
    auto it = floors_.find(floor);
    return it != floors_.end() && it->second.built;
}

int GridSystem::builtFloorCount() const {
    int count = 0;
    for (const auto& [floor, data] : floors_) {
        if (data.built) {
            ++count;
        }
    }
    return count;
}

Result<bool> GridSystem::placeTenant(TileCoord anchor, TenantType type, int width, EntityId entity) {
    // 유효성 검사
    if (!isValidCoord(anchor) || width <= 0 || width > floorWidth_) {
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }
    
    // 층이 건설되었는지 확인
    if (!isFloorBuilt(anchor.floor)) {
        return Result<bool>::failure(ErrorCode::FloorNotBuilt);
    }
    
    // 범위 초과 확인
    if (anchor.x + width > floorWidth_) {
        return Result<bool>::failure(ErrorCode::OutOfRange);
    }
    
    // 모든 타일이 비어있는지 확인 (엘리베이터 샤프트 포함)
    for (int i = 0; i < width; ++i) {
        TileCoord pos{anchor.x + i, anchor.floor};
        if (!isTileEmpty(pos)) {
            return Result<bool>::failure(ErrorCode::InvalidArgument);
        }
    }
    
    // 타일 배치
    auto& floorData = floors_[anchor.floor];
    for (int i = 0; i < width; ++i) {
        TileCoord pos{anchor.x + i, anchor.floor};
        TileData tile;
        tile.tenantType = type;
        tile.tenantEntity = entity;
        tile.isAnchor = (i == 0);
        tile.tileWidth = width;
        floorData.tiles[tileIndex(pos.x)] = tile;
        
        // 이벤트 발행
        Event tileEvent;
        tileEvent.type = EventType::TileOccupied;
        tileEvent.payload = pos;
        eventBus_.publish(tileEvent);
    }
    
    Event tenantEvent;
    tenantEvent.type = EventType::TenantPlaced;
    tenantEvent.payload = anchor;
    eventBus_.publish(tenantEvent);
    
    return Result<bool>::success(true);
}

Result<bool> GridSystem::removeTenant(TileCoord anyTile) {
    // 유효성 검사
    if (!isValidCoord(anyTile)) {
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }
    
    // 타일 정보 가져오기
    auto tileOpt = getTile(anyTile);
    if (!tileOpt || tileOpt->tenantType == TenantType::COUNT) {
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }
    
    // anchor 찾기
    auto anchorOpt = findAnchor(anyTile);
    if (!anchorOpt) {
        return Result<bool>::failure(ErrorCode::SystemError);
    }
    
    TileCoord anchor = *anchorOpt;
    auto anchorTile = getTile(anchor);
    if (!anchorTile) {
        return Result<bool>::failure(ErrorCode::SystemError);
    }
    
    int width = anchorTile->tileWidth;
    
    // 모든 타일 제거
    auto& floorData = floors_[anchor.floor];
    for (int i = 0; i < width; ++i) {
        TileCoord pos{anchor.x + i, anchor.floor};
        floorData.tiles[tileIndex(pos.x)] = TileData();
        
        // 이벤트 발행
        Event vacateEvent;
        vacateEvent.type = EventType::TileVacated;
        vacateEvent.payload = pos;
        eventBus_.publish(vacateEvent);
    }
    
    Event removeEvent;
    removeEvent.type = EventType::TenantRemoved;
    removeEvent.payload = anchor;
    eventBus_.publish(removeEvent);
    
    return Result<bool>::success(true);
}

std::optional<TileData> GridSystem::getTile(TileCoord pos) const {
    if (!isValidCoord(pos)) {
        return std::nullopt;
    }
    
    auto it = floors_.find(pos.floor);
    if (it == floors_.end() || !it->second.built) {
        return std::nullopt;
    }
    
    return it->second.tiles[tileIndex(pos.x)];
}

bool GridSystem::isTileEmpty(TileCoord pos) const {
    auto tile = getTile(pos);
    return tile.has_value() && tile->tenantType == TenantType::COUNT && !tile->isElevatorShaft;
}

bool GridSystem::isValidCoord(TileCoord pos) const {
    return pos.x >= 0 && pos.x < floorWidth_ && pos.floor >= 0 && pos.floor < maxFloors_;
}

std::vector<std::pair<TileCoord, TileData>> GridSystem::getFloorTiles(int floor) const {
    std::vector<std::pair<TileCoord, TileData>> result;
    
    auto it = floors_.find(floor);
    if (it == floors_.end() || !it->second.built) {
        return result;
    }
    
    const auto& tiles = it->second.tiles;
    for (int x = 0; x < floorWidth_; ++x) {
        TileCoord pos{x, floor};
        result.emplace_back(pos, tiles[tileIndex(x)]);
    }
    
    return result;
}

Result<bool> GridSystem::placeElevatorShaft(int x, int bottomFloor, int topFloor) {
    // 유효성 검사
    if (x < 0 || x >= floorWidth_ || 
        bottomFloor < 0 || bottomFloor >= maxFloors_ ||
        topFloor < 0 || topFloor >= maxFloors_ ||
        bottomFloor > topFloor) {
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }
    
    // 모든 층이 건설되었는지 확인
    for (int floor = bottomFloor; floor <= topFloor; ++floor) {
        if (!isFloorBuilt(floor)) {
            return Result<bool>::failure(ErrorCode::FloorNotBuilt);
        }
    }
    
    // 모든 타일이 비어있는지 확인
    for (int floor = bottomFloor; floor <= topFloor; ++floor) {
        TileCoord pos{x, floor};
        if (!isTileEmpty(pos)) {
            return Result<bool>::failure(ErrorCode::InvalidArgument);
        }
    }
    
    // 엘리베이터 샤프트 설치
    for (int floor = bottomFloor; floor <= topFloor; ++floor) {
        auto& floorData = floors_[floor];
        TileData& tile = floorData.tiles[tileIndex(x)];
        tile.isElevatorShaft = true;
        
        // 이벤트 발행
        Event shaftEvent;
        shaftEvent.type = EventType::TileOccupied;
        shaftEvent.payload = TileCoord{x, floor};
        eventBus_.publish(shaftEvent);
    }
    
    return Result<bool>::success(true);
}

bool GridSystem::isElevatorShaft(TileCoord pos) const {
    auto tile = getTile(pos);
    return tile.has_value() && tile->isElevatorShaft;
}

std::optional<TileCoord> GridSystem::findNearestEmpty(TileCoord from, int searchRadius) const {
    if (!isValidCoord(from) || searchRadius <= 0) {
        return std::nullopt;
    }

    if (!isFloorBuilt(from.floor)) {
        return std::nullopt;
    }

    // 탐색 우선순위:
    // 1. 같은 층(dy=0) 우선
    // 2. 가까운 층 순 (|dy| 오름차순)
    // 3. 수평 거리 오름차순 (|dx| 오름차순)
    // 4. 같은 거리면 좌 우선 (dx 음수 먼저) — 결정적 tie-break
    // 5. 층 방향은 위(+dy) 먼저 — 결정적 tie-break
    for (int floorDelta = 0; floorDelta <= searchRadius; ++floorDelta) {
        // 같은 층(floorDelta=0) → 위 → 아래 순
        int floorOffsets[3] = {0, floorDelta, -floorDelta};
        int numOffsets = (floorDelta == 0) ? 1 : 2;

        for (int fi = 0; fi < numOffsets; ++fi) {
            int dy = floorOffsets[fi];
            int targetFloor = from.floor + dy;

            if (targetFloor < 0 || targetFloor >= maxFloors_) continue;
            if (!isFloorBuilt(targetFloor)) continue;

            int maxXDelta = searchRadius - std::abs(dy);
            // 수평: 좌 우선 (dx=0, -1, 1, -2, 2 ... 순)
            for (int absDx = 0; absDx <= maxXDelta; ++absDx) {
                int dxList[2] = {-absDx, absDx};
                int numDx = (absDx == 0) ? 1 : 2;
                for (int xi = 0; xi < numDx; ++xi) {
                    int dx = dxList[xi];
                    TileCoord pos{from.x + dx, targetFloor};
                    if (pos == from) continue;  // 시작점 제외
                    if (!isValidCoord(pos)) continue;
                    if (isTileEmpty(pos)) return pos;
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<TileCoord> GridSystem::findAnchor(TileCoord anyTile) const {
    if (!isValidCoord(anyTile)) {
        return std::nullopt;
    }
    
    auto tile = getTile(anyTile);
    if (!tile || tile->tenantType == TenantType::COUNT) {
        return std::nullopt;
    }
    
    // 왼쪽으로 이동하며 anchor 찾기
    int x = anyTile.x;
    while (x >= 0) {
        TileCoord pos{x, anyTile.floor};
        auto currentTile = getTile(pos);
        if (!currentTile || currentTile->tenantEntity != tile->tenantEntity) {
            // 다른 테넌트를 만나면 중지
            break;
        }
        if (currentTile->isAnchor) {
            return pos;
        }
        --x;
    }
    
    // 오른쪽으로 이동하며 anchor 찾기
    x = anyTile.x + 1;
    while (x < floorWidth_) {
        TileCoord pos{x, anyTile.floor};
        auto currentTile = getTile(pos);
        if (!currentTile || currentTile->tenantEntity != tile->tenantEntity) {
            // 다른 테넌트를 만나면 중지
            break;
        }
        if (currentTile->isAnchor) {
            return pos;
        }
        ++x;
    }
    
    return std::nullopt;
}

size_t GridSystem::tileIndex(int x) const {
    return static_cast<size_t>(x);
}

} // namespace vse