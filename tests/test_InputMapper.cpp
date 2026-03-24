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
