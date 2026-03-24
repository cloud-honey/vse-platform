#include "core/ConfigManager.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <cassert>

namespace vse {

Result<bool> ConfigManager::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<bool>::failure(ErrorCode::FileNotFound);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    // exception 없이 JSON 파싱
    auto result = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (result.is_discarded()) {
        return Result<bool>::failure(ErrorCode::JsonParseError);
    }
    
    data_ = result;
    loaded_ = true;
    return Result<bool>::success(true);
}

const nlohmann::json* ConfigManager::find(const std::string& key) const {
    const nlohmann::json* current = &data_;
    
    size_t start = 0;
    size_t end = key.find('.');
    
    while (end != std::string::npos) {
        std::string part = key.substr(start, end - start);
        if (!current->contains(part)) {
            return nullptr;
        }
        current = &(*current)[part];
        start = end + 1;
        end = key.find('.', start);
    }
    
    std::string lastPart = key.substr(start);
    if (!current->contains(lastPart)) {
        return nullptr;
    }
    
    return &(*current)[lastPart];
}

bool ConfigManager::has(const std::string& key) const {
    return find(key) != nullptr;
}

int ConfigManager::getInt(const std::string& key, int defaultVal) const {
    assert(loaded_ && "ConfigManager::getInt() called before loadFromFile()");
    const nlohmann::json* node = find(key);
    if (!node) return defaultVal;
    if (node->is_number_integer()) return node->get<int>();
    // float 키에 getInt 호출 — 타입 불일치 경고
    if (node->is_number_float()) {
        spdlog::warn("ConfigManager::getInt('{}') — key is float, returning defaultVal. Use getFloat() instead.", key);
    }
    return defaultVal;
}

float ConfigManager::getFloat(const std::string& key, float defaultVal) const {
    const nlohmann::json* node = find(key);
    if (node && node->is_number_float()) {
        return node->get<float>();
    }
    // Also accept integer values as float
    if (node && node->is_number_integer()) {
        return static_cast<float>(node->get<int>());
    }
    return defaultVal;
}

std::string ConfigManager::getString(const std::string& key, const std::string& defaultVal) const {
    const nlohmann::json* node = find(key);
    if (node && node->is_string()) {
        return node->get<std::string>();
    }
    return defaultVal;
}

bool ConfigManager::getBool(const std::string& key, bool defaultVal) const {
    const nlohmann::json* node = find(key);
    if (node && node->is_boolean()) {
        return node->get<bool>();
    }
    return defaultVal;
}

const nlohmann::json& ConfigManager::raw() const {
    return data_;
}

} // namespace vse