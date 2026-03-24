#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <nlohmann/json.hpp>
#include <string>

namespace vse {

// Loads game_config.json at startup. Provides typed getters.
// Hot-reload for balance.json via ContentRegistry (not ConfigManager).
class ConfigManager {
public:
    Result<bool> loadFromFile(const std::string& path);

    // Typed getters with defaults
    int getInt(const std::string& key, int defaultVal = 0) const;
    float getFloat(const std::string& key, float defaultVal = 0.0f) const;
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;
    bool getBool(const std::string& key, bool defaultVal = false) const;

    // Direct JSON access for nested values
    const nlohmann::json& raw() const;

    // Check if key exists
    bool has(const std::string& key) const;

private:
    nlohmann::json data_;
    
    // Helper function to navigate nested JSON with dot notation
    const nlohmann::json* find(const std::string& key) const;
};

} // namespace vse