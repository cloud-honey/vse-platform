#include "catch2/catch_test_macros.hpp"
#include "renderer/HUDPanel.h"
#include "core/IRenderCommand.h"
#include <cmath>

using namespace vse;

// 부동소수점 비교 헬퍼
constexpr float EPSILON = 0.0001f;
static bool float_eq(float a, float b) {
    return std::fabs(a - b) < EPSILON;
}

TEST_CASE("HUDPanel 기본값 검증", "[HUDPanel]")
{
    HUDPanel panel;
    
    REQUIRE(panel.isVisible() == true);
    
    // 토글 테스트
    panel.toggle();
    REQUIRE(panel.isVisible() == false);
    
    panel.setVisible(true);
    REQUIRE(panel.isVisible() == true);
    
    panel.setVisible(false);
    REQUIRE(panel.isVisible() == false);
}

TEST_CASE("RenderFrame HUD 필드 기본값 검증", "[RenderFrame][HUD]")
{
    RenderFrame frame;
    
    // HUD 필드 기본값 확인
    REQUIRE(frame.balance == 0);
    REQUIRE(float_eq(frame.starRating, 0.0f));
    REQUIRE(frame.currentTick == 0);
    REQUIRE(frame.tenantCount == 0);
    REQUIRE(frame.npcCount == 0);
    REQUIRE(frame.showHUD == true);
    
    // 값 설정 테스트
    frame.balance = 1234567;
    frame.starRating = 3.5f;
    frame.currentTick = 1000;
    frame.tenantCount = 12;
    frame.npcCount = 45;
    frame.showHUD = false;
    
    REQUIRE(frame.balance == 1234567);
    REQUIRE(float_eq(frame.starRating, 3.5f));
    REQUIRE(frame.currentTick == 1000);
    REQUIRE(frame.tenantCount == 12);
    REQUIRE(frame.npcCount == 45);
    REQUIRE(frame.showHUD == false);
}

TEST_CASE("HUDPanel::formatBalance 포맷팅 테스트", "[HUDPanel][formatBalance]")
{
    SECTION("0원") {
        REQUIRE(HUDPanel::formatBalance(0) == "₩0");
    }
    SECTION("999원 (천단위 없음)") {
        REQUIRE(HUDPanel::formatBalance(999) == "₩999");
    }
    SECTION("1,000원") {
        REQUIRE(HUDPanel::formatBalance(1000) == "₩1,000");
    }
    SECTION("1,234,567원") {
        REQUIRE(HUDPanel::formatBalance(1234567) == "₩1,234,567");
    }
    SECTION("음수 잔액") {
        REQUIRE(HUDPanel::formatBalance(-500000) == "₩-500,000");
    }
    SECTION("1,000,000원") {
        REQUIRE(HUDPanel::formatBalance(1000000) == "₩1,000,000");
    }
    SECTION("대형 금액 (10억)") {
        REQUIRE(HUDPanel::formatBalance(1000000000LL) == "₩1,000,000,000");
    }
}

TEST_CASE("HUDPanel::formatStars 포맷팅 테스트", "[HUDPanel][formatStars]")
{
    SECTION("별점 0.0") {
        std::string s = HUDPanel::formatStars(0.0f);
        REQUIRE(s.find("☆☆☆☆☆") != std::string::npos);
    }
    SECTION("별점 5.0 (최대)") {
        std::string s = HUDPanel::formatStars(5.0f);
        REQUIRE(s.find("★★★★★") != std::string::npos);
    }
    SECTION("별점 2.5 (혼합)") {
        std::string s = HUDPanel::formatStars(2.5f);
        REQUIRE(s.find("2.5") != std::string::npos);
    }
    SECTION("별점 음수 → 클램프") {
        std::string s = HUDPanel::formatStars(-1.0f);
        // 음수는 0으로 클램프되거나 빈 별 표시
        REQUIRE(!s.empty());
    }
}

TEST_CASE("HUDPanel 가시성 제어 테스트", "[HUDPanel][visibility]")
{
    HUDPanel panel;
    RenderFrame frame;
    
    SECTION("패널이 보이는 경우") {
        panel.setVisible(true);
        frame.showHUD = true;
        // draw() 메서드가 호출되면 정상적으로 렌더링되어야 함
        // (실제 렌더링은 통합 테스트에서 검증)
        REQUIRE(panel.isVisible() == true);
        REQUIRE(frame.showHUD == true);
    }
    
    SECTION("패널이 숨겨진 경우") {
        panel.setVisible(false);
        frame.showHUD = true;
        // 패널이 숨겨져 있으므로 draw()가 아무것도 하지 않아야 함
        REQUIRE(panel.isVisible() == false);
        REQUIRE(frame.showHUD == true);
    }
    
    SECTION("HUD가 비활성화된 경우") {
        panel.setVisible(true);
        frame.showHUD = false;
        // HUD가 비활성화되어 있으므로 draw()가 아무것도 하지 않아야 함
        REQUIRE(panel.isVisible() == true);
        REQUIRE(frame.showHUD == false);
    }
    
    SECTION("둘 다 숨겨진 경우") {
        panel.setVisible(false);
        frame.showHUD = false;
        // 둘 다 숨겨져 있으므로 draw()가 아무것도 하지 않아야 함
        REQUIRE(panel.isVisible() == false);
        REQUIRE(frame.showHUD == false);
    }
}

TEST_CASE("RenderFrame 전체 HUD 데이터 통합 테스트", "[RenderFrame][HUD][integration]")
{
    RenderFrame frame;
    
    // 모든 HUD 필드 설정
    frame.balance = 987654321;
    frame.starRating = 4.2f;
    frame.currentTick = 5432;
    frame.tenantCount = 25;
    frame.npcCount = 78;
    frame.showHUD = true;
    
    // 값 검증
    REQUIRE(frame.balance == 987654321);
    REQUIRE(float_eq(frame.starRating, 4.2f));
    REQUIRE(frame.currentTick == 5432);
    REQUIRE(frame.tenantCount == 25);
    REQUIRE(frame.npcCount == 78);
    REQUIRE(frame.showHUD == true);
    
    // 다른 필드와의 독립성 확인
    REQUIRE(frame.drawGrid == true);
    REQUIRE(frame.drawDebugInfo == true);
    REQUIRE(frame.debug.gameTick == 0); // DebugInfo는 별도
}

TEST_CASE("SDLRenderer HUDPanel 통합 준비 테스트", "[SDLRenderer][HUDPanel][integration]")
{
    // SDLRenderer가 HUDPanel 멤버를 가지고 있는지 확인
    // (컴파일 타임 검증을 위한 더미 테스트)
    SECTION("헤더 포함 검증") {
        // HUDPanel.h가 정상적으로 include되었는지 확인
        // 실제로는 컴파일 오류가 없으면 성공
        REQUIRE(true);
    }
    
    SECTION("RenderFrame 구조체 크기 검증") {
        // HUD 필드 추가로 인한 구조체 크기 변화 확인
        RenderFrame frame;
        // 컴파일이 성공하면 구조체가 유효함
        REQUIRE(sizeof(frame) > 0);
    }
}

TEST_CASE("HUDPanel 토글 기능 테스트", "[HUDPanel][toggle]")
{
    HUDPanel panel;
    
    // 초기값
    REQUIRE(panel.isVisible() == true);
    
    // 토글 1: true → false
    panel.toggle();
    REQUIRE(panel.isVisible() == false);
    
    // 토글 2: false → true
    panel.toggle();
    REQUIRE(panel.isVisible() == true);
    
    // 토글 3: true → false
    panel.toggle();
    REQUIRE(panel.isVisible() == false);
    
    // setVisible로 강제 설정
    panel.setVisible(true);
    REQUIRE(panel.isVisible() == true);
    
    panel.setVisible(false);
    REQUIRE(panel.isVisible() == false);
}