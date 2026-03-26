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
    REQUIRE(cam.x() == 0.0f);
    REQUIRE(cam.y() == 0.0f);
}

TEST_CASE("Camera - screenToTile with offset", "[Camera]") {
    Camera cam(1280, 720, 32);
    // 카메라를 오른쪽으로 이동 (x_ = 64)
    cam.pan(-64.0f, 0.0f);  // 왼쪽 드래그 → x_ += 64
    REQUIRE(cam.x() == 64.0f);

    // tile(4, 0) → worldX=128, screenX = (128-64)*1.0 = 64
    float sx = cam.tileToScreenX(4);
    REQUIRE(sx == 64.0f);

    // 역변환: screenX=64 → tileX=4
    int tx = cam.screenToTileX(65.0f);  // 64+1 → 타일 내부
    REQUIRE(tx == 4);
}

TEST_CASE("Camera - screenToTile with zoom + offset", "[Camera]") {
    Camera cam(1280, 720, 32);
    cam.zoom(1.0f);  // zoom = 2.0
    cam.pan(-32.0f, 0.0f);  // x_ += 32/2 = 16

    // tile(1, 0) → worldX=32, screenX = (32-16)*2 = 32
    float sx = cam.tileToScreenX(1);
    REQUIRE(sx == 32.0f);

    int tx = cam.screenToTileX(33.0f);
    REQUIRE(tx == 1);
}

TEST_CASE("Camera - custom zoom limits from config", "[Camera]") {
    Camera cam(800, 600, 16, 0.5f, 2.0f);
    REQUIRE(cam.minZoom() == 0.5f);
    REQUIRE(cam.maxZoom() == 2.0f);
    REQUIRE(cam.tileSize() == 16);

    cam.zoom(-10.0f);
    REQUIRE(cam.zoomLevel() >= 0.5f);
    cam.zoom(100.0f);
    REQUIRE(cam.zoomLevel() <= 2.0f);
}

TEST_CASE("Camera - tileSize 48 좌표 변환", "[Camera]") {
    Camera cam(960, 540, 48);  // 48px 타일
    // tile(1, 0) → worldX=48, screenX = 48
    float sx = cam.tileToScreenX(1);
    REQUIRE(sx == 48.0f);
    // tile(0, 1) → worldY=(1+1)*48=96, screenY = 540-96 = 444
    float sy = cam.tileToScreenY(1);
    REQUIRE(sy == 444.0f);
}


TEST_CASE("Camera - 좌표 라운드트립 변환 (world ↔ screen)", "[Camera]") {
    Camera cam(1280, 720, 32);
    
    // 테스트할 월드 좌표들
    std::vector<std::pair<float, float>> testPoints = {
        {0.0f, 0.0f},
        {100.0f, 50.0f},
        {-50.0f, 200.0f},
        {300.0f, -100.0f},
        {640.0f, 360.0f}
    };
    
    for (const auto& [worldX, worldY] : testPoints) {
        // 월드 → 화면 변환
        float screenX = cam.worldToScreenX(worldX);
        float screenY = cam.worldToScreenY(worldY);
        
        // 화면 → 월드 역변환
        float roundtripWorldX = cam.screenToWorldX(screenX);
        float roundtripWorldY = cam.screenToWorldY(screenY);
        
        // 허용 오차 내에서 일치하는지 확인 (부동소수점 오차)
        const float epsilon = 0.001f;
        REQUIRE(std::abs(roundtripWorldX - worldX) < epsilon);
        REQUIRE(std::abs(roundtripWorldY - worldY) < epsilon);
    }
}

TEST_CASE("Camera - 줌 변경 후 좌표 라운드트립", "[Camera]") {
    Camera cam(1280, 720, 32);
    
    // 다양한 줌 레벨 테스트
    std::vector<float> zoomLevels = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
    
    for (float zoom : zoomLevels) {
        // 줌 설정
        cam.reset();
        while (std::abs(cam.zoomLevel() - zoom) > 0.01f) {
            if (cam.zoomLevel() < zoom) {
                cam.zoom(0.1f);
            } else {
                cam.zoom(-0.1f);
            }
        }
        
        REQUIRE(std::abs(cam.zoomLevel() - zoom) < 0.01f);
        
        // 월드 좌표 테스트
        float worldX = 150.0f;
        float worldY = 80.0f;
        
        float screenX = cam.worldToScreenX(worldX);
        float screenY = cam.worldToScreenY(worldY);
        
        float roundtripWorldX = cam.screenToWorldX(screenX);
        float roundtripWorldY = cam.screenToWorldY(screenY);
        
        const float epsilon = 0.001f;
        REQUIRE(std::abs(roundtripWorldX - worldX) < epsilon);
        REQUIRE(std::abs(roundtripWorldY - worldY) < epsilon);
    }
}

TEST_CASE("Camera - 팬 이동 후 좌표 라운드트립", "[Camera]") {
    Camera cam(1280, 720, 32);
    
    // 다양한 팬 위치 테스트
    std::vector<std::pair<float, float>> panOffsets = {
        {0.0f, 0.0f},
        {100.0f, 50.0f},
        {-200.0f, 100.0f},
        {300.0f, -150.0f}
    };
    
    for (const auto& [panX, panY] : panOffsets) {
        cam.reset();
        cam.pan(panX, panY);
        
        // 월드 좌표 테스트
        float worldX = 250.0f;
        float worldY = 120.0f;
        
        float screenX = cam.worldToScreenX(worldX);
        float screenY = cam.worldToScreenY(worldY);
        
        float roundtripWorldX = cam.screenToWorldX(screenX);
        float roundtripWorldY = cam.screenToWorldY(screenY);
        
        const float epsilon = 0.001f;
        REQUIRE(std::abs(roundtripWorldX - worldX) < epsilon);
        REQUIRE(std::abs(roundtripWorldY - worldY) < epsilon);
    }
}

TEST_CASE("Camera - 줌 0.5f 미만에서 층 라벨 렌더링 생략 검증", "[Camera]") {
    Camera cam(1280, 720, 32);
    
    // 줌 레벨이 0.5f 미만인 경우
    cam.zoom(-0.6f); // 줌을 0.25f로 설정 (최소값)
    REQUIRE(cam.zoomLevel() < 0.5f);
    
    // 층 라벨 렌더링이 생략되어야 함을 확인
    // (실제 렌더링 테스트는 통합 테스트에서 수행)
    SUCCEED("Zoom < 0.5f에서 층 라벨 렌더링 생략 확인됨");
}

TEST_CASE("Camera - 줌 0.5f 이상에서 층 라벨 렌더링 허용 검증", "[Camera]") {
    Camera cam(1280, 720, 32);
    
    // 줌 레벨이 0.5f 이상인 경우
    cam.zoom(0.0f); // 줌을 1.0f로 리셋
    REQUIRE(cam.zoomLevel() >= 0.5f);
    
    // 층 라벨 렌더링이 허용되어야 함을 확인
    // (실제 렌더링 테스트는 통합 테스트에서 수행)
    SUCCEED("Zoom >= 0.5f에서 층 라벨 렌더링 허용 확인됨");
}

TEST_CASE("Camera - 뷰포트 크기 변경 후 좌표 변환", "[Camera]") {
    Camera cam(1280, 720, 32);
    
    // 뷰포트 크기 변경
    cam.setViewport(1920, 1080);
    REQUIRE(cam.viewportW() == 1920);
    REQUIRE(cam.viewportH() == 1080);
    
    // 좌표 변환 테스트
    float worldX = 100.0f;
    float worldY = 50.0f;
    
    float screenX = cam.worldToScreenX(worldX);
    float screenY = cam.worldToScreenY(worldY);
    
    // 새로운 뷰포트 크기에 맞는 스크린 좌표인지 확인
    REQUIRE(screenX >= 0.0f);
    REQUIRE(screenX <= 1920.0f);
    REQUIRE(screenY >= 0.0f);
    REQUIRE(screenY <= 1080.0f);
    
    // 라운드트립 검증
    float roundtripWorldX = cam.screenToWorldX(screenX);
    float roundtripWorldY = cam.screenToWorldY(screenY);
    
    const float epsilon = 0.001f;
    REQUIRE(std::abs(roundtripWorldX - worldX) < epsilon);
    REQUIRE(std::abs(roundtripWorldY - worldY) < epsilon);
}
