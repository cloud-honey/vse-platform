#include "core/IRenderCommand.h"
#include "core/InputTypes.h"
#include <catch2/catch_test_macros.hpp>

using namespace vse;

// ── RenderFrame 구조체 테스트 ────────────────────────────

TEST_CASE("RenderFrame - 기본 생성", "[RenderFrame]") {
    RenderFrame frame;
    REQUIRE(frame.tiles.empty());
    REQUIRE(frame.elevators.empty());
    REQUIRE(frame.texts.empty());
    REQUIRE(frame.drawGrid == true);
    REQUIRE(frame.tileSize == 32);
}

TEST_CASE("RenderFrame - 타일 추가", "[RenderFrame]") {
    RenderFrame frame;
    frame.tiles.push_back(RenderTile{0, 0, Color::fromRGBA(255, 0, 0)});
    frame.tiles.push_back(RenderTile{1, 0, Color::fromRGBA(0, 255, 0)});
    REQUIRE(frame.tiles.size() == 2);
    REQUIRE(frame.tiles[0].color.r == 255);
    REQUIRE(frame.tiles[1].color.g == 255);
}

TEST_CASE("RenderFrame - 엘리베이터 추가", "[RenderFrame]") {
    RenderFrame frame;
    RenderElevator re;
    re.shaftX = 5;
    re.currentFloor = 3;
    re.interpolatedFloor = 3.0f;
    re.state = ElevatorState::MovingUp;
    re.direction = ElevatorDirection::Up;
    re.passengerCount = 2;
    re.capacity = 8;
    re.color = Color::fromRGBA(79, 142, 247);
    frame.elevators.push_back(re);
    REQUIRE(frame.elevators.size() == 1);
    REQUIRE(frame.elevators[0].shaftX == 5);
}

TEST_CASE("Color - fromRGBA", "[Color]") {
    auto c = Color::fromRGBA(10, 20, 30, 40);
    REQUIRE(c.r == 10);
    REQUIRE(c.g == 20);
    REQUIRE(c.b == 30);
    REQUIRE(c.a == 40);
}

TEST_CASE("Color - fromRGBA 기본 알파", "[Color]") {
    auto c = Color::fromRGBA(100, 200, 50);
    REQUIRE(c.a == 255);
}

// ── GameCommand 테스트 ──────────────────────────────────

TEST_CASE("GameCommand - makeQuit", "[GameCommand]") {
    auto cmd = GameCommand::makeQuit();
    REQUIRE(cmd.type == CommandType::Quit);
}

TEST_CASE("GameCommand - makeBuildFloor", "[GameCommand]") {
    auto cmd = GameCommand::makeBuildFloor(5);
    REQUIRE(cmd.type == CommandType::BuildFloor);
    REQUIRE(cmd.buildFloor.floor == 5);
}

TEST_CASE("GameCommand - makeCameraPan", "[GameCommand]") {
    auto cmd = GameCommand::makeCameraPan(10.0f, -20.0f);
    REQUIRE(cmd.type == CommandType::CameraPan);
    REQUIRE(cmd.cameraPan.deltaX == 10.0f);
    REQUIRE(cmd.cameraPan.deltaY == -20.0f);
}

TEST_CASE("GameCommand - makeCameraZoom", "[GameCommand]") {
    auto cmd = GameCommand::makeCameraZoom(0.5f);
    REQUIRE(cmd.type == CommandType::CameraZoom);
    REQUIRE(cmd.cameraZoom.zoomDelta == 0.5f);
}

TEST_CASE("GameCommand - makeSetSpeed", "[GameCommand]") {
    auto cmd = GameCommand::makeSetSpeed(4);
    REQUIRE(cmd.type == CommandType::SetSpeed);
    REQUIRE(cmd.setSpeed.speedMultiplier == 4);
}

TEST_CASE("GameCommand - makeSelectTile", "[GameCommand]") {
    auto cmd = GameCommand::makeSelectTile(3, 7);
    REQUIRE(cmd.type == CommandType::SelectTile);
    REQUIRE(cmd.selectTile.tileX == 3);
    REQUIRE(cmd.selectTile.tileFloor == 7);
}
