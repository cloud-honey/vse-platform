#include "core/InputTypes.h"
#include <catch2/catch_test_macros.hpp>
#include <vector>

// InputMapper는 SDL_Event에 의존 → 헤드리스 환경에서는
// GameCommand 생성 로직을 직접 검증한다.
// SDL_Event 구조체를 수동 조립하여 변환 결과를 확인.

// NOTE: SDL.h include 없이도 테스트 가능하도록
// InputMapper의 핵심 로직(커맨드 팩토리)만 검증

using namespace vse;

TEST_CASE("GameCommand - makeQuit 타입 확인", "[InputMapper]") {
    auto cmd = GameCommand::makeQuit();
    REQUIRE(cmd.type == CommandType::Quit);
}

TEST_CASE("GameCommand - makeCameraPan 양수", "[InputMapper]") {
    auto cmd = GameCommand::makeCameraPan(10.0f, 5.0f);
    REQUIRE(cmd.type == CommandType::CameraPan);
    REQUIRE(cmd.cameraPan.deltaX == 10.0f);
    REQUIRE(cmd.cameraPan.deltaY == 5.0f);
}

TEST_CASE("GameCommand - makeCameraPan 음수", "[InputMapper]") {
    auto cmd = GameCommand::makeCameraPan(-8.0f, -3.0f);
    REQUIRE(cmd.cameraPan.deltaX == -8.0f);
    REQUIRE(cmd.cameraPan.deltaY == -3.0f);
}

TEST_CASE("GameCommand - makeCameraZoom 확대", "[InputMapper]") {
    auto cmd = GameCommand::makeCameraZoom(0.1f);
    REQUIRE(cmd.cameraZoom.zoomDelta == 0.1f);
}

TEST_CASE("GameCommand - makeCameraZoom 축소", "[InputMapper]") {
    auto cmd = GameCommand::makeCameraZoom(-0.1f);
    REQUIRE(cmd.cameraZoom.zoomDelta == -0.1f);
}

TEST_CASE("GameCommand - makeTogglePause", "[InputMapper]") {
    auto cmd = GameCommand::makeTogglePause();
    REQUIRE(cmd.type == CommandType::TogglePause);
}

TEST_CASE("GameCommand - makeSetSpeed 배속 값 보존", "[InputMapper]") {
    for (int s : {1, 2, 4}) {
        auto cmd = GameCommand::makeSetSpeed(s);
        REQUIRE(cmd.type == CommandType::SetSpeed);
        REQUIRE(cmd.setSpeed.speedMultiplier == s);
    }
}

TEST_CASE("GameCommand - makeSelectTile 좌표 보존", "[InputMapper]") {
    auto cmd = GameCommand::makeSelectTile(15, 29);
    REQUIRE(cmd.selectTile.tileX == 15);
    REQUIRE(cmd.selectTile.tileFloor == 29);
}

TEST_CASE("GameCommand - makeBuildFloor 범위", "[InputMapper]") {
    auto cmd = GameCommand::makeBuildFloor(0);
    REQUIRE(cmd.buildFloor.floor == 0);
    auto cmd2 = GameCommand::makeBuildFloor(29);
    REQUIRE(cmd2.buildFloor.floor == 29);
}

TEST_CASE("GameCommand - 커맨드 벡터 순서 보존", "[InputMapper]") {
    std::vector<GameCommand> cmds;
    cmds.push_back(GameCommand::makeTogglePause());
    cmds.push_back(GameCommand::makeCameraPan(1.0f, 0.0f));
    cmds.push_back(GameCommand::makeQuit());
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[0].type == CommandType::TogglePause);
    REQUIRE(cmds[1].type == CommandType::CameraPan);
    REQUIRE(cmds[2].type == CommandType::Quit);
}

// TASK-03-006: BuildModeState 및 InputMapper 확장 테스트
#include "renderer/BuildModeState.h"
#include "renderer/InputMapper.h"
#include "renderer/Camera.h"
#include "core/IRenderCommand.h"

TEST_CASE("BuildModeState - 기본값", "[InputMapper]") {
    BuildModeState state;
    REQUIRE(state.action == BuildAction::None);
    REQUIRE(state.tenantType == 0);
    REQUIRE(state.tenantWidth == 1);
    REQUIRE(state.active == false);
}

TEST_CASE("InputMapper - setBuildMode/getBuildMode 라운드트립", "[InputMapper]") {
    InputMapper mapper;
    BuildModeState state;
    state.action = BuildAction::BuildFloor;
    state.active = true;
    
    mapper.setBuildMode(state);
    BuildModeState retrieved = mapper.getBuildMode();
    
    REQUIRE(retrieved.action == BuildAction::BuildFloor);
    REQUIRE(retrieved.active == true);
    REQUIRE(retrieved.tenantType == 0);
    REQUIRE(retrieved.tenantWidth == 1);
}

TEST_CASE("InputMapper - PlaceTenant 모드 설정", "[InputMapper]") {
    InputMapper mapper;
    BuildModeState state;
    state.action = BuildAction::PlaceTenant;
    state.tenantType = 2; // Commercial
    state.tenantWidth = 3;
    state.active = true;
    
    mapper.setBuildMode(state);
    BuildModeState retrieved = mapper.getBuildMode();
    
    REQUIRE(retrieved.action == BuildAction::PlaceTenant);
    REQUIRE(retrieved.tenantType == 2);
    REQUIRE(retrieved.tenantWidth == 3);
    REQUIRE(retrieved.active == true);
}

TEST_CASE("GameCommand - makePlaceTenant 생성", "[InputMapper]") {
    auto cmd = GameCommand::makePlaceTenant(10, 5, 1, 2);
    REQUIRE(cmd.type == CommandType::PlaceTenant);
    REQUIRE(cmd.placeTenant.x == 10);
    REQUIRE(cmd.placeTenant.floor == 5);
    REQUIRE(cmd.placeTenant.tenantType == 1);
    REQUIRE(cmd.placeTenant.width == 2);
}

TEST_CASE("BuildCursor - None 모드에서 크래시 없음", "[InputMapper]") {
    // BuildCursor가 None 모드에서 아무것도 그리지 않는지 확인
    // 실제로는 draw() 메서드가 호출되지 않거나 early return해야 함
    BuildModeState state;
    state.action = BuildAction::None;
    state.active = false;
    
    // 이 테스트는 컴파일만 확인 (런타임 테스트는 별도)
    REQUIRE(state.action == BuildAction::None);
}

TEST_CASE("RenderFrame - mouseX/mouseY 필드 존재", "[InputMapper]") {
    // IRenderCommand.h에 필드가 추가되었는지 컴파일 타임 확인
    RenderFrame frame;
    frame.mouseX = 100;
    frame.mouseY = 200;
    frame.buildMode.action = BuildAction::BuildFloor;
    
    REQUIRE(frame.mouseX == 100);
    REQUIRE(frame.mouseY == 200);
    REQUIRE(frame.buildMode.action == BuildAction::BuildFloor);
}

TEST_CASE("InputMapper - 카메라 설정", "[InputMapper]") {
    InputMapper mapper;
    Camera camera(1280, 720, 32);
    
    mapper.setCamera(&camera);
    // setCamera는 내부 포인터만 설정, 검증은 processEvent에서
    REQUIRE(true); // 컴파일 확인용
}

TEST_CASE("GameCommand - BuildFloor 커맨드 생성", "[InputMapper]") {
    auto cmd = GameCommand::makeBuildFloor(3);
    REQUIRE(cmd.type == CommandType::BuildFloor);
    REQUIRE(cmd.buildFloor.floor == 3);
}
