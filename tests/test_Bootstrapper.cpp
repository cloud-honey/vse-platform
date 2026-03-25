/**
 * @file test_Bootstrapper.cpp
 * @layer Core Runtime 테스트
 * @task TASK-02-001 (수정: SimClock 통합, VSE_TESTING 가드 적용)
 *
 * headless 단위 테스트 — SDL 윈도우 없이 Domain만 초기화.
 * initDomainOnly() + testGet*() / testProcessCommands() 사용.
 *
 * VSE_TESTING 매크로가 정의된 빌드에서만 테스트 전용 접근자 노출됨.
 */
#define VSE_TESTING  // 테스트 전용 접근자 활성화
#include <catch2/catch_test_macros.hpp>
#include "core/Bootstrapper.h"
#include "core/InputTypes.h"

using namespace vse;

TEST_CASE("Bootstrapper - Domain 초기화", "[bootstrapper]") {
    Bootstrapper b;
    REQUIRE(b.initDomainOnly("assets/config/game_config.json") == true);
    REQUIRE(b.testGetRunning() == true);
    REQUIRE(b.testGetPaused() == false);
    REQUIRE(b.testGetSpeed() == 1);
}

TEST_CASE("Bootstrapper - TogglePause 커맨드", "[bootstrapper]") {
    Bootstrapper b;
    REQUIRE(b.initDomainOnly("assets/config/game_config.json"));

    REQUIRE(b.testGetPaused() == false);

    std::vector<GameCommand> cmds = { GameCommand::makeTogglePause() };
    b.testProcessCommands(cmds);
    REQUIRE(b.testGetPaused() == true);

    b.testProcessCommands(cmds);
    REQUIRE(b.testGetPaused() == false);
}

TEST_CASE("Bootstrapper - SetSpeed 커맨드", "[bootstrapper]") {
    Bootstrapper b;
    REQUIRE(b.initDomainOnly("assets/config/game_config.json"));

    REQUIRE(b.testGetSpeed() == 1);

    std::vector<GameCommand> cmds = { GameCommand::makeSetSpeed(4) };
    b.testProcessCommands(cmds);
    REQUIRE(b.testGetSpeed() == 4);
}

TEST_CASE("Bootstrapper - Quit 커맨드", "[bootstrapper]") {
    Bootstrapper b;
    REQUIRE(b.initDomainOnly("assets/config/game_config.json"));

    REQUIRE(b.testGetRunning() == true);

    std::vector<GameCommand> cmds = { GameCommand::makeQuit() };
    b.testProcessCommands(cmds);
    REQUIRE(b.testGetRunning() == false);
}

TEST_CASE("Bootstrapper - BuildFloor 커맨드 crash 없음", "[bootstrapper]") {
    Bootstrapper b;
    REQUIRE(b.initDomainOnly("assets/config/game_config.json"));

    GameCommand cmd{};
    cmd.type = CommandType::BuildFloor;
    cmd.buildFloor.floor = 10;

    std::vector<GameCommand> cmds = { cmd };
    b.testProcessCommands(cmds);
    REQUIRE(b.testGetRunning() == true);  // crash 없이 완료
}
