#include <catch2/catch_test_macros.hpp>
#include "core/Bootstrapper.h"
#include "core/InputTypes.h"

using namespace vse;

/**
 * Bootstrapper 테스트 (headless — SDL 윈도우 없이 Domain만)
 *
 * initDomainOnly()로 SDL 없이 Domain 시스템만 조립.
 * testGet*() / testProcessCommands() 헬퍼로 내부 상태 확인.
 */

TEST_CASE("Bootstrapper - Domain 초기화", "[bootstrapper]") {
    Bootstrapper b;
    REQUIRE(b.initDomainOnly("assets/config/game_config.json") == true);
    REQUIRE(b.testGetRunning() == true);
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
    b.testProcessCommands(cmds);  // crash 없이 완료
    REQUIRE(b.testGetRunning() == true);
}
