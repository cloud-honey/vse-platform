#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <nlohmann/json.hpp>
#include <string>

namespace vse {

/**
 * ConfigManager — game_config.json 로더 및 타입 게터.
 *
 * 사용 순서:
 *   1. loadFromFile(path) 호출
 *   2. getInt/getFloat/getString/getBool로 값 조회
 *
 * @note loadFromFile()이 성공적으로 호출되기 전에 getter를 호출하면
 *       빈 JSON 객체({})로 처리되어 모두 defaultVal을 반환합니다.
 *       isLoaded()로 상태 확인 후 사용하세요.
 *
 * @note ConfigManager는 초기화 시 1회 로드 후 읽기 전용으로 사용합니다.
 *       게임 루프 내에서 getter를 반복 호출하는 대신,
 *       각 시스템 초기화 시점에 필요한 값을 로컬 변수에 캐싱하세요.
 *
 * Hot-reload는 ContentRegistry가 담당합니다 (balance.json 등).
 */
class ConfigManager {
public:
    Result<bool> loadFromFile(const std::string& path);

    // 로드 상태 확인
    bool isLoaded() const { return loaded_; }

    // Typed getters — dot notation 지원 (예: "simulation.tickRateMs")
    // 키 없거나 타입 불일치 시 defaultVal 반환
    int         getInt   (const std::string& key, int defaultVal = 0) const;
    float       getFloat (const std::string& key, float defaultVal = 0.0f) const;
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;
    bool        getBool  (const std::string& key, bool defaultVal = false) const;

    // 전체 JSON 직접 접근 (중첩 구조 직접 탐색 시)
    const nlohmann::json& raw() const;

    // 키 존재 여부 (dot notation 지원)
    bool has(const std::string& key) const;

private:
    nlohmann::json data_;
    bool loaded_ = false;

    // dot notation으로 중첩 JSON 노드 탐색
    const nlohmann::json* find(const std::string& key) const;
};

} // namespace vse
