#include <catch2/catch_test_macros.hpp>
#include "core/ContentRegistry.h"
#include "core/Types.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>

using namespace vse;
namespace fs = std::filesystem;

// ── Helper: the real assets directory ────────────────────────────────────
static std::string assetDir() {
#ifdef VSE_PROJECT_ROOT
    return std::string(VSE_PROJECT_ROOT) + "/assets";
#else
    return "./assets";
#endif
}

// ── Helper: write a minimal balance.json to a temp dir ───────────────────
static std::string setupTempContentDir(const std::string& suffix = "") {
    std::string tmpDir = fs::temp_directory_path().string()
                         + "/vse_test_content" + suffix;
    fs::create_directories(tmpDir + "/data");
    std::ofstream f(tmpDir + "/data/balance.json");
    f << R"({
  "tenants": {
    "office":      { "rent": 500, "maxOccupants": 6 },
    "residential": { "rent": 300, "maxOccupants": 3 },
    "commercial":  { "rent": 800, "maxOccupants": 0 }
  },
  "economy": { "startingBalance": 1000000 }
})";
    f.close();
    return tmpDir;
}

// ── Test cases ────────────────────────────────────────────────────────────

TEST_CASE("ContentRegistry: loadContentPack with valid directory succeeds", "[ContentRegistry]") {
    ContentRegistry reg;
    auto result = reg.loadContentPack(assetDir());
    REQUIRE(result.ok());
    REQUIRE(result.value == true);
}

TEST_CASE("ContentRegistry: getBalanceData returns loaded data", "[ContentRegistry]") {
    ContentRegistry reg;
    auto result = reg.loadContentPack(assetDir());
    REQUIRE(result.ok());

    const auto& bal = reg.getBalanceData();
    REQUIRE(bal.is_object());
    REQUIRE(bal.contains("tenants"));
    REQUIRE(bal.contains("economy"));
}

TEST_CASE("ContentRegistry: getTenantDef returns correct data for Office", "[ContentRegistry]") {
    ContentRegistry reg;
    REQUIRE(reg.loadContentPack(assetDir()).ok());

    const auto& td = reg.getTenantDef(TenantType::Office);
    REQUIRE(td.is_object());
    REQUIRE(td.contains("rent"));
    REQUIRE(td["rent"].get<int>() == 500);
}

TEST_CASE("ContentRegistry: getTenantDef returns correct data for Residential", "[ContentRegistry]") {
    ContentRegistry reg;
    REQUIRE(reg.loadContentPack(assetDir()).ok());

    const auto& td = reg.getTenantDef(TenantType::Residential);
    REQUIRE(td.is_object());
    REQUIRE(td.contains("rent"));
    REQUIRE(td["rent"].get<int>() == 300);
}

TEST_CASE("ContentRegistry: getTenantDef returns correct data for Commercial", "[ContentRegistry]") {
    ContentRegistry reg;
    REQUIRE(reg.loadContentPack(assetDir()).ok());

    const auto& td = reg.getTenantDef(TenantType::Commercial);
    REQUIRE(td.is_object());
    REQUIRE(td.contains("rent"));
    REQUIRE(td["rent"].get<int>() == 800);
}

TEST_CASE("ContentRegistry: getTenantDef with invalid type returns empty JSON", "[ContentRegistry]") {
    ContentRegistry reg;
    REQUIRE(reg.loadContentPack(assetDir()).ok());

    // TenantType::COUNT is out of range
    const auto& td = reg.getTenantDef(TenantType::COUNT);
    REQUIRE(td.is_object());
    REQUIRE(td.empty());
}

TEST_CASE("ContentRegistry: checkAndReload returns false when no files changed", "[ContentRegistry]") {
    ContentRegistry reg;
    REQUIRE(reg.loadContentPack(assetDir()).ok());

    bool reloaded = reg.checkAndReload();
    REQUIRE(reloaded == false);
}

TEST_CASE("ContentRegistry: checkAndReload returns true and updates data when file is modified", "[ContentRegistry]") {
    std::string tmpDir = setupTempContentDir("_reload");
    ContentRegistry reg;
    REQUIRE(reg.loadContentPack(tmpDir).ok());

    // Verify initial value
    const auto& bal = reg.getBalanceData();
    REQUIRE(bal["economy"]["startingBalance"].get<int>() == 1000000);

    // Give the filesystem a moment then overwrite the file with new content
    // We need mtime to actually change, so sleep at least 1 file-system tick
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        std::ofstream f(tmpDir + "/data/balance.json");
        f << R"({
  "tenants": {
    "office":      { "rent": 999, "maxOccupants": 6 },
    "residential": { "rent": 300, "maxOccupants": 3 },
    "commercial":  { "rent": 800, "maxOccupants": 0 }
  },
  "economy": { "startingBalance": 9999999 }
})";
    }

    // Force mtime to update (touch via last_write_time)
    auto newTime = fs::file_time_type::clock::now();
    std::error_code ec;
    fs::last_write_time(tmpDir + "/data/balance.json", newTime, ec);

    bool reloaded = reg.checkAndReload();
    REQUIRE(reloaded == true);

    const auto& bal2 = reg.getBalanceData();
    REQUIRE(bal2["economy"]["startingBalance"].get<int>() == 9999999);
    REQUIRE(bal2["tenants"]["office"]["rent"].get<int>() == 999);

    // Cleanup
    fs::remove_all(tmpDir);
}

TEST_CASE("ContentRegistry: onReload callback is invoked on reload", "[ContentRegistry]") {
    std::string tmpDir = setupTempContentDir("_callback");
    ContentRegistry reg;
    REQUIRE(reg.loadContentPack(tmpDir).ok());

    std::atomic<int> callCount{0};
    reg.onReload([&callCount]() { callCount++; });

    // No change yet
    REQUIRE(reg.checkAndReload() == false);
    REQUIRE(callCount == 0);

    // Modify the file
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::ofstream f(tmpDir + "/data/balance.json");
        f << R"({ "tenants": {}, "economy": { "startingBalance": 42 } })";
    }
    auto newTime = fs::file_time_type::clock::now();
    std::error_code ec;
    fs::last_write_time(tmpDir + "/data/balance.json", newTime, ec);

    REQUIRE(reg.checkAndReload() == true);
    REQUIRE(callCount == 1);

    // Second checkAndReload with no further change → no extra callback
    REQUIRE(reg.checkAndReload() == false);
    REQUIRE(callCount == 1);

    fs::remove_all(tmpDir);
}

TEST_CASE("ContentRegistry: loadContentPack with invalid directory returns failure", "[ContentRegistry]") {
    ContentRegistry reg;
    auto result = reg.loadContentPack("/nonexistent/path/that/does/not/exist");
    REQUIRE_FALSE(result.ok());
}

TEST_CASE("ContentRegistry: getBalanceData returns empty when not loaded", "[ContentRegistry]") {
    ContentRegistry reg;  // nothing loaded
    const auto& bal = reg.getBalanceData();
    REQUIRE(bal.is_object());
    REQUIRE(bal.empty());
}

TEST_CASE("ContentRegistry: multiple onReload callbacks all fire", "[ContentRegistry]") {
    std::string tmpDir = setupTempContentDir("_multicb");
    ContentRegistry reg;
    REQUIRE(reg.loadContentPack(tmpDir).ok());

    int cb1 = 0, cb2 = 0;
    reg.onReload([&cb1]() { cb1++; });
    reg.onReload([&cb2]() { cb2++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::ofstream f(tmpDir + "/data/balance.json");
        f << R"({ "tenants": {}, "economy": { "startingBalance": 1 } })";
    }
    auto newTime = fs::file_time_type::clock::now();
    std::error_code ec;
    fs::last_write_time(tmpDir + "/data/balance.json", newTime, ec);

    REQUIRE(reg.checkAndReload() == true);
    REQUIRE(cb1 == 1);
    REQUIRE(cb2 == 1);

    fs::remove_all(tmpDir);
}
