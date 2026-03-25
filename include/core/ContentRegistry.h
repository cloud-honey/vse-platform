#pragma once

#include "core/Error.h"
#include "core/Types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>

namespace vse {

// ── ContentRegistry ────────────────────────────────────────────────────────
// Manages game content files (balance.json etc.) with hot-reload support.
// Registered callbacks fire whenever any tracked file changes on disk.
// checkAndReload() is intended to be called every 600 ticks by Bootstrapper.
class ContentRegistry {
public:
    ContentRegistry() = default;
    ~ContentRegistry() = default;

    // Non-copyable, non-movable (holds callbacks by value)
    ContentRegistry(const ContentRegistry&) = delete;
    ContentRegistry& operator=(const ContentRegistry&) = delete;

    // ── Content loading ───────────────────────────────────────────────────
    // Loads balance.json from <directory>/data/balance.json.
    // Records file mtime for hot-reload tracking.
    // Returns failure if directory or file is missing / unreadable.
    Result<bool> loadContentPack(const std::string& directory);

    // ── Data accessors ────────────────────────────────────────────────────
    // Returns a const ref to the full balance data JSON object.
    // If not yet loaded, returns a static empty JSON object.
    // ⚠️ Reference validity: returned ref is invalidated by the next checkAndReload()
    //    that modifies balance data. Do NOT store across tick boundaries.
    //    Read values at point of use and cache locally if needed.
    const nlohmann::json& getBalanceData() const;

    // Returns balance data for the given TenantType from content_["balance"]["tenants"][key].
    // Mapping: Office→"office", Residential→"residential", Commercial→"commercial".
    // If not found (or type is COUNT/unknown), returns a static empty JSON object.
    //
    // ⚠️ Phase 1 deviation: Design Spec defines getTenantDef() as returning content
    //    definitions from tenants.json (displayName, sprite, description). Phase 1
    //    loads only balance.json, so this returns economy balance values (rent, maintenance, etc.).
    //    When tenants.json is added (Phase 2+), this method should return tenants.json data
    //    and a separate getBalanceTenantConfig() should serve the economic values.
    // ⚠️ Same reference validity caveat as getBalanceData().
    const nlohmann::json& getTenantDef(TenantType type) const;

    // ── Hot-reload ────────────────────────────────────────────────────────
    // Checks mtime of each tracked file. Re-reads & calls callbacks if changed.
    // Returns true if at least one file was reloaded.
    bool checkAndReload();

    // Registers a callback to be invoked whenever any content file is reloaded.
    void onReload(std::function<void()> callback);

private:
    std::unordered_map<std::string, nlohmann::json>    content_;
    std::string                                         contentDir_;
    std::vector<std::function<void()>>                  reloadCallbacks_;
    std::unordered_map<std::string, int64_t>            fileMtimes_;
};

} // namespace vse
