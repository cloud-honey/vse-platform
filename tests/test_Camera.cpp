#include "renderer/Camera.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace vse;

TEST_CASE("Camera - 초기 상태", "[Camera]") {
    Camera cam(1280, 720, 32);
    REQUIRE(cam.viewportW() == 1280);
    REQUIRE(cam.viewportH() == 720);
    REQUIRE(cam.tileSize() == 32);
    REQUIRE(cam.zoomLevel() == 1.0f);
}

TEST_CASE("Camera - tileToScreen 기본 변환", "[Camera]") {
    Camera cam(1280, 720, 32);
    // 카메라 (0,0), 줌 1.0
    // tile(0,0) → 화면: x=0, y=720-32=688
    float sx = cam.tileToScreenX(0);
    float sy = cam.tileToScreenY(0);
    REQUIRE(sx == 0.0f);
    REQUIRE(sy == 688.0f);  // viewportH - (floor+1)*tileSize*zoom = 720-32 = 688
}

TEST_CASE("Camera - tileToScreen floor 증가 시 y 감소", "[Camera]") {
    Camera cam(1280, 720, 32);
    float y0 = cam.tileToScreenY(0);  // 688
    float y1 = cam.tileToScreenY(1);  // 656
    float y2 = cam.tileToScreenY(2);  // 624
    REQUIRE(y0 > y1);
    REQUIRE(y1 > y2);
    REQUIRE(y0 - y1 == 32.0f);
}

TEST_CASE("Camera - 줌 적용", "[Camera]") {
    Camera cam(1280, 720, 32);
    cam.zoom(1.0f);  // zoom = 2.0
    REQUIRE(cam.zoomLevel() == 2.0f);
    // tile(1,0) → worldX=32, screenX = (32-0)*2.0 = 64
    float sx = cam.tileToScreenX(1);
    REQUIRE(sx == 64.0f);
}

TEST_CASE("Camera - 줌 범위 제한", "[Camera]") {
    Camera cam(1280, 720, 32);
    cam.zoom(-10.0f);  // 최소 0.25
    REQUIRE(cam.zoomLevel() >= 0.25f);
    cam.zoom(100.0f);  // 최대 4.0
    REQUIRE(cam.zoomLevel() <= 4.0f);
}

TEST_CASE("Camera - 팬 이동", "[Camera]") {
    Camera cam(1280, 720, 32);
    cam.pan(32.0f, 0.0f);  // 화면 오른쪽 드래그 → 월드 왼쪽 이동
    REQUIRE(cam.x() < 0.0f);
}

TEST_CASE("Camera - reset", "[Camera]") {
    Camera cam(1280, 720, 32);
    cam.pan(100, 100);
    cam.zoom(1.0f);
    cam.reset();
    REQUIRE(cam.x() == 0.0f);
    REQUIRE(cam.y() == 0.0f);
    REQUIRE(cam.zoomLevel() == 1.0f);
}

TEST_CASE("Camera - screenToTile 역변환", "[Camera]") {
    Camera cam(1280, 720, 32);
    // tile(3, 2) → screen
    float sx = cam.tileToScreenX(3);   // 96
    float sy = cam.tileToScreenY(2);   // 720 - 3*32 = 624

    // screen → tile (타일 내부 클릭 → 같은 타일 반환)
    int tx = cam.screenToTileX(sx + 16.0f);
    int tf = cam.screenToTileFloor(sy + 16.0f);
    REQUIRE(tx == 3);
    REQUIRE(tf == 2);
}

TEST_CASE("Camera - centerOn", "[Camera]") {
    Camera cam(1280, 720, 32);
    cam.centerOn(640.0f, 360.0f);
    // 카메라 x_ = 640 - 1280/(2*1) = 0
    // 카메라 y_ = 360 - 720/(2*1) = 0
    REQUIRE(cam.x() == 0.0f);
    REQUIRE(cam.y() == 0.0f);
}
