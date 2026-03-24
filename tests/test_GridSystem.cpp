#include "domain/GridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <catch2/catch_test_macros.hpp>

using namespace vse;

// 테스트용 헬퍼 — 실제 ConfigManager + game_config.json 사용
static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

#define MAKE_GRID(name) \
    EventBus bus_##name; \
    ConfigManager cfg_##name; \
    cfg_##name.loadFromFile(cfgPath()); \
    GridSystem name(bus_##name, cfg_##name)

TEST_CASE("GridSystem - 초기 상태", "[GridSystem]") {
    MAKE_GRID(grid);
    REQUIRE(grid.isFloorBuilt(0) == true);
    REQUIRE(grid.isFloorBuilt(1) == false);
    REQUIRE(grid.builtFloorCount() == 1);
}

TEST_CASE("GridSystem - buildFloor 성공", "[GridSystem]") {
    MAKE_GRID(grid);
    auto result = grid.buildFloor(1);
    REQUIRE(result.ok() == true);
    REQUIRE(grid.isFloorBuilt(1) == true);
    REQUIRE(grid.builtFloorCount() == 2);
}

TEST_CASE("GridSystem - buildFloor 중복", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    auto result = grid.buildFloor(1);
    REQUIRE(result.ok() == false);
}

TEST_CASE("GridSystem - 단일 타일 테넌트 배치", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    auto result = grid.placeTenant({5, 1}, TenantType::Office, 1, EntityId{100});
    REQUIRE(result.ok() == true);

    auto tile = grid.getTile({5, 1});
    REQUIRE(tile.has_value() == true);
    REQUIRE(tile->tenantType == TenantType::Office);
    REQUIRE(tile->tenantEntity == EntityId{100});
    REQUIRE(tile->isAnchor == true);
    REQUIRE(tile->tileWidth == 1);
}

TEST_CASE("GridSystem - 멀티 타일 테넌트 배치 (width=3)", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    auto result = grid.placeTenant({5, 1}, TenantType::Residential, 3, EntityId{200});
    REQUIRE(result.ok() == true);

    auto anchor = grid.getTile({5, 1});
    REQUIRE(anchor.has_value() == true);
    REQUIRE(anchor->isAnchor == true);
    REQUIRE(anchor->tileWidth == 3);

    auto aux1 = grid.getTile({6, 1});
    REQUIRE(aux1.has_value() == true);
    REQUIRE(aux1->isAnchor == false);
    REQUIRE(aux1->tenantEntity == EntityId{200});

    auto aux2 = grid.getTile({7, 1});
    REQUIRE(aux2.has_value() == true);
    REQUIRE(aux2->isAnchor == false);
    REQUIRE(aux2->tenantEntity == EntityId{200});
}

TEST_CASE("GridSystem - 미건설 층에 테넌트 배치 → FloorNotBuilt", "[GridSystem]") {
    MAKE_GRID(grid);
    auto result = grid.placeTenant({5, 2}, TenantType::Office, 1, EntityId{300});
    REQUIRE(result.ok() == false);
    REQUIRE(result.error == ErrorCode::FloorNotBuilt);
}

TEST_CASE("GridSystem - 범위 초과 테넌트 배치 → OutOfRange", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    // tilesPerFloor=20, x=18+width=5 → 23 > 20
    auto result = grid.placeTenant({18, 1}, TenantType::Office, 5, EntityId{400});
    REQUIRE(result.ok() == false);
    REQUIRE(result.error == ErrorCode::OutOfRange);
}

TEST_CASE("GridSystem - anchor 타일로 테넌트 제거", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.placeTenant({5, 1}, TenantType::Office, 1, EntityId{500});
    REQUIRE(grid.isTileEmpty({5, 1}) == false);

    auto result = grid.removeTenant({5, 1});
    REQUIRE(result.ok() == true);
    REQUIRE(grid.isTileEmpty({5, 1}) == true);
}

TEST_CASE("GridSystem - auxiliary 타일로 테넌트 제거 (anchor 자동 탐색)", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.placeTenant({5, 1}, TenantType::Residential, 3, EntityId{600});

    // (7,1) = 세번째 타일(auxiliary)로 제거
    auto result = grid.removeTenant({7, 1});
    REQUIRE(result.ok() == true);
    REQUIRE(grid.isTileEmpty({5, 1}) == true);
    REQUIRE(grid.isTileEmpty({6, 1}) == true);
    REQUIRE(grid.isTileEmpty({7, 1}) == true);
}

TEST_CASE("GridSystem - isTileEmpty", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    REQUIRE(grid.isTileEmpty({5, 1}) == true);
    grid.placeTenant({5, 1}, TenantType::Office, 1, EntityId{700});
    REQUIRE(grid.isTileEmpty({5, 1}) == false);
}

TEST_CASE("GridSystem - isValidCoord 경계", "[GridSystem]") {
    MAKE_GRID(grid);
    // game_config: maxFloors=30, tilesPerFloor=20
    REQUIRE(grid.isValidCoord({0,  0})  == true);
    REQUIRE(grid.isValidCoord({19, 0})  == true);
    REQUIRE(grid.isValidCoord({20, 0})  == false);  // x 범위 초과
    REQUIRE(grid.isValidCoord({0,  30}) == false);  // floor 범위 초과
    REQUIRE(grid.isValidCoord({-1, 0})  == false);  // 음수
}

TEST_CASE("GridSystem - getFloorTiles", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.placeTenant({5, 1}, TenantType::Office, 2, EntityId{800});

    auto tiles = grid.getFloorTiles(1);
    REQUIRE(tiles.empty() == false);

    bool foundAnchor = false;
    for (auto& [coord, tile] : tiles) {
        if (coord.x == 5 && tile.isAnchor) {
            foundAnchor = true;
            REQUIRE(tile.tenantType == TenantType::Office);
        }
    }
    REQUIRE(foundAnchor == true);
}

TEST_CASE("GridSystem - 엘리베이터 샤프트 설치", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.buildFloor(2);

    auto result = grid.placeElevatorShaft(10, 0, 2);
    REQUIRE(result.ok() == true);
    REQUIRE(grid.isElevatorShaft({10, 0}) == true);
    REQUIRE(grid.isElevatorShaft({10, 1}) == true);
    REQUIRE(grid.isElevatorShaft({10, 2}) == true);
    REQUIRE(grid.isElevatorShaft({10, 3}) == false);
}

TEST_CASE("GridSystem - isElevatorShaft", "[GridSystem]") {
    MAKE_GRID(grid);
    REQUIRE(grid.isElevatorShaft({5, 0}) == false);
    grid.buildFloor(1);
    grid.placeElevatorShaft(5, 0, 1);
    REQUIRE(grid.isElevatorShaft({5, 0}) == true);
    REQUIRE(grid.isElevatorShaft({5, 1}) == true);
}

TEST_CASE("GridSystem - 엘리베이터 샤프트에 테넌트 배치 거부", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.placeElevatorShaft(5, 0, 1);
    auto result = grid.placeTenant({5, 0}, TenantType::Office, 1, EntityId{900});
    REQUIRE(result.ok() == false);
}

TEST_CASE("GridSystem - anchor 내부 탐색 동작 (removeTenant 간접 검증)", "[GridSystem]") {
    // findAnchor()는 private helper — removeTenant()가 내부에서 호출
    // auxiliary 타일로 removeTenant() 호출 시 anchor까지 포함해 전체 span 제거됨을 검증
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.placeTenant({5, 1}, TenantType::Residential, 3, EntityId{1000});

    // (7,1) = auxiliary — 제거 후 (5,1), (6,1), (7,1) 전체 비어야 함
    auto result = grid.removeTenant({7, 1});
    REQUIRE(result.ok() == true);
    REQUIRE(grid.isTileEmpty({5, 1}) == true);
    REQUIRE(grid.isTileEmpty({6, 1}) == true);
    REQUIRE(grid.isTileEmpty({7, 1}) == true);
}

TEST_CASE("GridSystem - findNearestEmpty", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.placeTenant({5, 1}, TenantType::Office, 1, EntityId{1100});

    // (5,1)은 점유됨 — 인근 빈 타일 반환
    auto nearest = grid.findNearestEmpty({5, 1}, 5);
    REQUIRE(nearest.has_value() == true);
    REQUIRE(grid.isTileEmpty(nearest.value()) == true);
}

// ── GPT-5.4 검토 후 추가 테스트 ──────────────────────────────────────────────

TEST_CASE("GridSystem - floor 0 로비 초기화", "[GridSystem]") {
    MAKE_GRID(grid);

    // 층 0 전 타일이 isLobby=true여야 함
    auto tiles = grid.getFloorTiles(0);
    REQUIRE(tiles.empty() == false);
    for (auto& [coord, tile] : tiles) {
        REQUIRE(tile.isLobby == true);
    }
}

TEST_CASE("GridSystem - floor 1은 로비 아님", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);

    auto tiles = grid.getFloorTiles(1);
    for (auto& [coord, tile] : tiles) {
        REQUIRE(tile.isLobby == false);
    }
}

TEST_CASE("GridSystem - getFloorTiles 반환 개수 = floorWidth", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.placeTenant({5, 1}, TenantType::Office, 2, EntityId{999});

    auto tiles = grid.getFloorTiles(1);
    REQUIRE(static_cast<int>(tiles.size()) == grid.floorWidth());
}

TEST_CASE("GridSystem - findNearestEmpty: 같은 층 우선", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);
    grid.buildFloor(2);

    // (5,1) 점유, (6,1) 비어있음, (5,2)도 비어있음
    // → 같은 층(1)의 (6,1)이 먼저 반환돼야 함
    grid.placeTenant({5, 1}, TenantType::Office, 1, EntityId{100});
    auto nearest = grid.findNearestEmpty({5, 1}, 5);
    REQUIRE(nearest.has_value() == true);
    REQUIRE(nearest->floor == 1);  // 같은 층 우선
}

TEST_CASE("GridSystem - findNearestEmpty: 좌 우선 tie-break", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);

    // (5,1) 점유, (4,1)과 (6,1) 동일 거리 — 좌(4) 먼저
    grid.placeTenant({5, 1}, TenantType::Office, 1, EntityId{200});
    auto nearest = grid.findNearestEmpty({5, 1}, 3);
    REQUIRE(nearest.has_value() == true);
    REQUIRE(nearest->floor == 1);
    REQUIRE(nearest->x == 4);  // 좌 우선
}

TEST_CASE("GridSystem - findNearestEmpty: searchRadius 경계", "[GridSystem]") {
    MAKE_GRID(grid);
    grid.buildFloor(1);

    // (5,1) 채우고 반경 0 탐색 → nullopt
    grid.placeTenant({5, 1}, TenantType::Office, 1, EntityId{300});
    auto result = grid.findNearestEmpty({5, 1}, 0);
    REQUIRE(result.has_value() == false);
}
