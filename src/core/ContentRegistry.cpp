#include "core/ContentRegistry.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

namespace vse {

namespace fs = std::filesystem;

// ── Internal helpers ──────────────────────────────────────────────────────

static int64_t getFileMtime(const std::string& path) {
    std::error_code ec;
    auto ft = fs::last_write_time(path, ec);
    if (ec) return -1;
    // Convert file_time_type to int64_t (nanoseconds since epoch via duration cast)
    auto dur = ft.time_since_epoch();
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count());
}

static nlohmann::json readJsonFile(const std::string& path, bool& ok) {
    std::ifstream f(path);
    if (!f.is_open()) {
        ok = false;
        return {};
    }
    nlohmann::json j = nlohmann::json::parse(f, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        ok = false;
        return {};
    }
    ok = true;
    return j;
}

// ── Static empties ────────────────────────────────────────────────────────

static const nlohmann::json& emptyJson() {
    static const nlohmann::json kEmpty = nlohmann::json::object();
    return kEmpty;
}

// ── ContentRegistry implementation ────────────────────────────────────────

Result<bool> ContentRegistry::loadContentPack(const std::string& directory) {
    std::string balancePath = directory + "/data/balance.json";

    // Check directory existence
    std::error_code ec;
    if (!fs::is_directory(directory, ec) || ec) {
        spdlog::warn("[ContentRegistry] loadContentPack: directory not found: {}", directory);
        return Result<bool>::failure(ErrorCode::FileNotFound);
    }

    bool ok = false;
    nlohmann::json j = readJsonFile(balancePath, ok);
    if (!ok) {
        spdlog::warn("[ContentRegistry] loadContentPack: failed to read balance.json at: {}", balancePath);
        return Result<bool>::failure(ErrorCode::FileReadError);
    }

    content_["balance"] = std::move(j);
    contentDir_ = directory;

    int64_t mtime = getFileMtime(balancePath);
    fileMtimes_[balancePath] = mtime;

    spdlog::info("[ContentRegistry] Loaded balance.json from {}", balancePath);
    return Result<bool>::success(true);
}

const nlohmann::json& ContentRegistry::getBalanceData() const {
    auto it = content_.find("balance");
    if (it == content_.end()) return emptyJson();
    return it->second;
}

const nlohmann::json& ContentRegistry::getTenantDef(TenantType type) const {
    const char* key = nullptr;
    switch (type) {
        case TenantType::Office:      key = "office";      break;
        case TenantType::Residential: key = "residential"; break;
        case TenantType::Commercial:  key = "commercial";  break;
        default:                      return emptyJson();
    }

    auto balIt = content_.find("balance");
    if (balIt == content_.end()) return emptyJson();

    const nlohmann::json& bal = balIt->second;
    if (!bal.contains("tenants")) return emptyJson();

    const nlohmann::json& tenants = bal["tenants"];
    if (!tenants.contains(key)) return emptyJson();

    return tenants[key];
}

bool ContentRegistry::checkAndReload() {
    bool anyReloaded = false;

    for (auto& [path, prevMtime] : fileMtimes_) {
        int64_t curMtime = getFileMtime(path);
        if (curMtime == -1 || curMtime == prevMtime) continue;

        bool ok = false;
        nlohmann::json j = readJsonFile(path, ok);
        if (!ok) {
            spdlog::warn("[ContentRegistry] checkAndReload: failed to re-read: {}", path);
            continue;
        }

        // Determine which content key this path belongs to (currently only "balance")
        std::string balancePath = contentDir_ + "/data/balance.json";
        if (path == balancePath) {
            content_["balance"] = std::move(j);
            spdlog::info("[ContentRegistry] Reloaded balance.json");
        }

        prevMtime = curMtime;
        anyReloaded = true;
    }

    if (anyReloaded) {
        for (auto& cb : reloadCallbacks_) {
            cb();
        }
    }

    return anyReloaded;
}

void ContentRegistry::onReload(std::function<void()> callback) {
    reloadCallbacks_.push_back(std::move(callback));
}

} // namespace vse
