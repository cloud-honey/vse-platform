#include "core/ConfigManager.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <fstream>
#include <filesystem>

using Catch::Matchers::WithinRel;

static std::string configPath() {
    // 빌드 디렉토리 기준 상위로 올라가서 프로젝트 루트 찾기
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

TEST_CASE("ConfigManager - 파일 로드 성공", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    
    REQUIRE(result.ok() == true);
    REQUIRE(result.value == true);
}

TEST_CASE("ConfigManager - 존재하지 않는 파일", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile("assets/config/nonexistent.json");
    
    REQUIRE(result.ok() == false);
    REQUIRE(result.error == vse::ErrorCode::FileNotFound);
}

TEST_CASE("ConfigManager - getInt dot notation", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    REQUIRE(config.getInt("simulation.tickRateMs") == 100);
    REQUIRE(config.getInt("grid.tileSizePx") == 32);
    REQUIRE(config.getInt("elevator.capacity") == 8);
}

TEST_CASE("ConfigManager - getFloat", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    // npc.satisfactionDecayRate 값 확인
    float decayRate = config.getFloat("npc.satisfactionDecayRate");
    REQUIRE_THAT(decayRate, WithinRel(0.01f, 0.01f));
    
    // rating.thresholds.oneStar 값 확인
    float oneStar = config.getFloat("rating.thresholds.oneStar");
    REQUIRE_THAT(oneStar, WithinRel(0.2f, 0.01f));
}

TEST_CASE("ConfigManager - getString", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    // _comment 키 확인
    std::string comment = config.getString("_comment");
    REQUIRE_FALSE(comment.empty());
    REQUIRE(comment.find("Tower Tycoon") != std::string::npos);
}

TEST_CASE("ConfigManager - getBool", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    // 없는 키에 default 값 반환
    REQUIRE(config.getBool("nonexistent.key") == false);
    REQUIRE(config.getBool("nonexistent.key", true) == true);
}

TEST_CASE("ConfigManager - has()", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    REQUIRE(config.has("simulation.tickRateMs") == true);
    REQUIRE(config.has("grid.tileSizePx") == true);
    REQUIRE(config.has("nonexistent.key") == false);
    REQUIRE(config.has("simulation.nonexistent") == false);
}

TEST_CASE("ConfigManager - raw()", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    const auto& json = config.raw();
    REQUIRE(json["grid"]["tileSizePx"] == 32);
    REQUIRE(json["simulation"]["maxFloors"] == 30);
}

TEST_CASE("ConfigManager - default fallback", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    // 없는 키에 default 값 반환
    REQUIRE(config.getInt("nonexistent.key", 42) == 42);
    REQUIRE_THAT(config.getFloat("nonexistent.key", 3.14f), WithinRel(3.14f, 0.01f));
    REQUIRE(config.getString("nonexistent.key", "default") == "default");
    REQUIRE(config.getBool("nonexistent.key", true) == true);
}

TEST_CASE("ConfigManager - 잘못된 JSON 파일", "[ConfigManager]") {
    // 임시 잘못된 JSON 파일 생성
    std::string tempPath = "test_invalid.json";
    {
        std::ofstream file(tempPath);
        file << "{ invalid json }";
    }
    
    vse::ConfigManager config;
    auto result = config.loadFromFile(tempPath);
    
    // 파일 삭제
    std::filesystem::remove(tempPath);
    
    REQUIRE(result.ok() == false);
    REQUIRE(result.error == vse::ErrorCode::JsonParseError);
}

TEST_CASE("ConfigManager - 중첩 키 접근", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    // 깊은 중첩 키 테스트
    REQUIRE_THAT(config.getFloat("rating.thresholds.oneStar"), WithinRel(0.2f, 0.01f));
    REQUIRE_THAT(config.getFloat("tenant.office.baseRent"), WithinRel(500.0f, 0.01f));
    REQUIRE_THAT(config.getFloat("tenant.residential.maintenanceCost"), WithinRel(80.0f, 0.01f));
}

TEST_CASE("ConfigManager - 타입 변환", "[ConfigManager]") {
    vse::ConfigManager config;
    auto result = config.loadFromFile(configPath());
    REQUIRE(result.ok() == true);
    
    // 정수 값을 float로 가져오기
    float tileSize = config.getFloat("grid.tileSizePx");
    REQUIRE_THAT(tileSize, WithinRel(32.0f, 0.01f));
    
    // 정수 값을 int로 가져오기
    int maxFloors = config.getInt("simulation.maxFloors");
    REQUIRE(maxFloors == 30);
}