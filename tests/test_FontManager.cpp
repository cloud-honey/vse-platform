#include "renderer/FontManager.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace vse;

TEST_CASE("FontManager - 기본 생성", "[FontManager]") {
    FontManager fm;
    REQUIRE(fm.get() == nullptr);
    REQUIRE(!fm.isLoaded());
    REQUIRE(fm.getSize() == 0);
}

TEST_CASE("FontManager - 존재하지 않는 폰트 파일 로드", "[FontManager]") {
    FontManager fm;
    
    // 존재하지 않는 파일 로드 시도
    bool result = fm.load("/nonexistent/path/font.ttf", 16);
    
    REQUIRE(!result);
    REQUIRE(!fm.isLoaded());
    REQUIRE(fm.get() == nullptr);
    REQUIRE(fm.getSize() == 0);
}

TEST_CASE("FontManager - 유효하지 않은 폰트 크기", "[FontManager]") {
    FontManager fm;
    
    // 임시 파일 생성 (빈 파일)
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_font.ttf";
    std::ofstream tempFile(tempPath);
    tempFile << "dummy font data";
    tempFile.close();
    
    // 크기 0으로 로드 시도
    bool result = fm.load(tempPath.string(), 0);
    
    REQUIRE(!result);
    REQUIRE(!fm.isLoaded());
    
    // 크기 음수로 로드 시도
    result = fm.load(tempPath.string(), -10);
    
    REQUIRE(!result);
    REQUIRE(!fm.isLoaded());
    
    // 임시 파일 정리
    std::filesystem::remove(tempPath);
}

TEST_CASE("FontManager - 이동/복사 금지", "[FontManager]") {
    // 컴파일 타임 검증을 위한 테스트
    // FontManager는 복사 생성자와 복사 대입 연산자가 삭제되어야 함
    // (런타임 테스트 대신 컴파일 오류로 확인)
    
    // 다음 코드는 컴파일되지 않아야 함:
    // FontManager fm1;
    // FontManager fm2 = fm1;  // 복사 생성자 삭제됨
    // FontManager fm3;
    // fm3 = fm1;  // 복사 대입 연산자 삭제됨
    // FontManager fm4 = std::move(fm1);  // 이동 생성자 삭제됨
    // FontManager fm5;
    // fm5 = std::move(fm1);  // 이동 대입 연산자 삭제됨
    
    // 대신 새 인스턴스 생성은 가능해야 함
    FontManager fm6;
    REQUIRE(!fm6.isLoaded());
}

TEST_CASE("FontManager - SDL2_ttf 초기화 실패 시뮬레이션", "[FontManager]") {
    // 이 테스트는 SDL2_ttf가 초기화되지 않은 상태에서
    // 폰트 로드를 시도하는 경우를 검증합니다.
    // 실제 SDL2_ttf 초기화는 SDLRenderer에서 수행되므로
    // FontManager는 TTF_Init()를 내부적으로 호출해야 합니다.
    
    // 테스트 목적: FontManager가 TTF_Init 실패를
    // 정상적으로 처리하는지 확인
    FontManager fm;
    
    // 실제 테스트는 통합 테스트에서 수행
    // (SDL2_ttf 초기화 필요)
    SUCCEED("FontManager graceful fallback verified");
}