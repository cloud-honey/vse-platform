#include "catch2/catch_test_macros.hpp"
#include "core/IRenderCommand.h"
#include <cmath>

using namespace vse;

// 부동소수점 비교 헬퍼
constexpr float EPSILON = 0.0001f;
bool float_eq(float a, float b) {
    return std::fabs(a - b) < EPSILON;
}

using namespace vse;

TEST_CASE("DebugInfo 기본값 검증", "[DebugInfo]")
{
    DebugInfo d;
    
    REQUIRE(d.gameTick == 0);
    REQUIRE(d.gameHour == 0);
    REQUIRE(d.gameMinute == 0);
    REQUIRE(d.gameDay == 1);
    REQUIRE(float_eq(d.simSpeed, 1.0f));
    REQUIRE(d.isPaused == false);
    REQUIRE(d.npcTotal == 0);
    REQUIRE(d.npcIdle == 0);
    REQUIRE(d.npcWorking == 0);
    REQUIRE(d.npcResting == 0);
    REQUIRE(d.elevatorCount == 0);
    REQUIRE(float_eq(d.avgSatisfaction, 100.0f));
    REQUIRE(float_eq(d.fps, 0.0f));
}

TEST_CASE("RenderFrame.debug 필드 존재 확인", "[RenderFrame][DebugInfo]")
{
    RenderFrame frame;
    
    // 기본값 확인
    REQUIRE(frame.debug.gameTick == 0);
    REQUIRE(frame.debug.gameDay == 1);
    REQUIRE(float_eq(frame.debug.simSpeed, 1.0f));
    REQUIRE(frame.debug.isPaused == false);
    
    // 다른 필드도 기본값 확인
    REQUIRE(frame.drawGrid == true);
    REQUIRE(frame.drawDebugInfo == true);
}

TEST_CASE("DebugInfo 값 설정/조회 검증", "[DebugInfo]")
{
    DebugInfo d;
    
    // 값 설정
    d.gameTick = 12345;
    d.gameHour = 14;
    d.gameMinute = 30;
    d.gameDay = 5;
    d.simSpeed = 2.5f;
    d.isPaused = true;
    d.npcTotal = 42;
    d.npcIdle = 10;
    d.npcWorking = 25;
    d.npcResting = 7;
    d.elevatorCount = 3;
    d.avgSatisfaction = 85.5f;
    d.fps = 60.0f;
    
    // 값 검증
    REQUIRE(d.gameTick == 12345);
    REQUIRE(d.gameHour == 14);
    REQUIRE(d.gameMinute == 30);
    REQUIRE(d.gameDay == 5);
    REQUIRE(float_eq(d.simSpeed, 2.5f));
    REQUIRE(d.isPaused == true);
    REQUIRE(d.npcTotal == 42);
    REQUIRE(d.npcIdle == 10);
    REQUIRE(d.npcWorking == 25);
    REQUIRE(d.npcResting == 7);
    REQUIRE(d.elevatorCount == 3);
    REQUIRE(float_eq(d.avgSatisfaction, 85.5f));
    REQUIRE(float_eq(d.fps, 60.0f));
}

TEST_CASE("isPaused 플래그 토글", "[DebugInfo]")
{
    DebugInfo d;
    
    REQUIRE(d.isPaused == false);
    
    // 토글
    d.isPaused = true;
    REQUIRE(d.isPaused == true);
    
    d.isPaused = false;
    REQUIRE(d.isPaused == false);
}

TEST_CASE("npc 카운트 합산 검증", "[DebugInfo]")
{
    DebugInfo d;
    
    // 각 상태별 카운트 설정
    d.npcIdle = 15;
    d.npcWorking = 20;
    d.npcResting = 5;
    
    // total은 별도 필드이므로 직접 설정해야 함
    d.npcTotal = 40;
    
    // 합계 검증 (total이 맞는지 확인)
    REQUIRE(d.npcTotal == 40);
    REQUIRE(d.npcIdle + d.npcWorking + d.npcResting == 40);
    
    // total이 다르게 설정된 경우
    d.npcTotal = 50;
    d.npcIdle = 20;
    d.npcWorking = 25;
    d.npcResting = 5;
    
    REQUIRE(d.npcTotal == 50);
    REQUIRE(d.npcIdle + d.npcWorking + d.npcResting == 50);
}

TEST_CASE("avgSatisfaction 범위 확인", "[DebugInfo]")
{
    DebugInfo d;
    
    // 기본값
    REQUIRE(float_eq(d.avgSatisfaction, 100.0f));
    
    // 유효한 범위 내 값 설정
    d.avgSatisfaction = 0.0f;
    REQUIRE(float_eq(d.avgSatisfaction, 0.0f));
    
    d.avgSatisfaction = 50.0f;
    REQUIRE(float_eq(d.avgSatisfaction, 50.0f));
    
    d.avgSatisfaction = 100.0f;
    REQUIRE(float_eq(d.avgSatisfaction, 100.0f));
    
    // 100% 초과도 가능 (보너스 등)
    d.avgSatisfaction = 120.0f;
    REQUIRE(float_eq(d.avgSatisfaction, 120.0f));
    
    // 음수도 가능 (불만족)
    d.avgSatisfaction = -10.0f;
    REQUIRE(float_eq(d.avgSatisfaction, -10.0f));
}

TEST_CASE("DebugInfo 복사 및 비교", "[DebugInfo]")
{
    DebugInfo d1;
    d1.gameTick = 100;
    d1.gameDay = 3;
    d1.simSpeed = 2.0f;
    d1.isPaused = true;
    d1.npcTotal = 30;
    d1.fps = 59.9f;
    
    // 복사 생성
    DebugInfo d2 = d1;
    
    // 값 동일성 확인
    REQUIRE(d2.gameTick == 100);
    REQUIRE(d2.gameDay == 3);
    REQUIRE(float_eq(d2.simSpeed, 2.0f));
    REQUIRE(d2.isPaused == true);
    REQUIRE(d2.npcTotal == 30);
    REQUIRE(float_eq(d2.fps, 59.9f));
    
    // 수정 후 독립성 확인
    d2.gameTick = 200;
    REQUIRE(d1.gameTick == 100);  // 원본 변경되지 않음
    REQUIRE(d2.gameTick == 200);  // 복사본만 변경
}

TEST_CASE("RenderFrame에 DebugInfo 포함된 전체 구조 테스트", "[RenderFrame][DebugInfo]")
{
    RenderFrame frame;
    
    // 기본값 설정
    frame.maxFloors = 30;
    frame.floorWidth = 40;
    frame.tileSize = 32;
    frame.drawGrid = true;
    frame.drawDebugInfo = true;
    
    // DebugInfo 설정
    frame.debug.gameTick = 54321;
    frame.debug.gameHour = 23;
    frame.debug.gameMinute = 59;
    frame.debug.gameDay = 7;
    frame.debug.simSpeed = 4.0f;
    frame.debug.isPaused = false;
    frame.debug.npcTotal = 50;
    frame.debug.npcIdle = 20;
    frame.debug.npcWorking = 25;
    frame.debug.npcResting = 5;
    frame.debug.elevatorCount = 4;
    frame.debug.avgSatisfaction = 92.3f;
    frame.debug.fps = 60.0f;
    
    // 전체 구조 검증
    REQUIRE(frame.maxFloors == 30);
    REQUIRE(frame.floorWidth == 40);
    REQUIRE(frame.tileSize == 32);
    REQUIRE(frame.drawGrid == true);
    REQUIRE(frame.drawDebugInfo == true);
    
    REQUIRE(frame.debug.gameTick == 54321);
    REQUIRE(frame.debug.gameHour == 23);
    REQUIRE(frame.debug.gameMinute == 59);
    REQUIRE(frame.debug.gameDay == 7);
    REQUIRE(float_eq(frame.debug.simSpeed, 4.0f));
    REQUIRE(frame.debug.isPaused == false);
    REQUIRE(frame.debug.npcTotal == 50);
    REQUIRE(frame.debug.npcIdle == 20);
    REQUIRE(frame.debug.npcWorking == 25);
    REQUIRE(frame.debug.npcResting == 5);
    REQUIRE(frame.debug.elevatorCount == 4);
    REQUIRE(float_eq(frame.debug.avgSatisfaction, 92.3f));
    REQUIRE(float_eq(frame.debug.fps, 60.0f));
}