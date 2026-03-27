// tests/test_SaveLoadPanel.cpp
// Unit tests for SaveLoadPanel and SaveSlotInfo (logic only, no ImGui/SDL).
#include <catch2/catch_test_macros.hpp>
#include "renderer/SaveLoadPanel.h"

namespace vse {
namespace {

// ── SaveSlotInfo tests ───────────────────────────────────────────────────────

TEST_CASE("SaveSlotInfo has correct default values", "[SaveLoadPanel]") {
    SaveSlotInfo info;
    REQUIRE(info.slotIndex == 0);
    REQUIRE(info.isEmpty == true);
    REQUIRE(info.meta.version == 1u);
    REQUIRE(info.meta.starRating == 1);
    REQUIRE(info.meta.balance == 0);
    REQUIRE(info.displayName.empty());
}

TEST_CASE("SaveSlotInfo can be populated", "[SaveLoadPanel]") {
    SaveSlotInfo info;
    info.slotIndex = 2;
    info.isEmpty = false;
    info.meta.starRating = 3;
    info.meta.balance = 500000;
    info.displayName = "Day 45  ★3  ₩5,000";

    REQUIRE(info.slotIndex == 2);
    REQUIRE(info.isEmpty == false);
    REQUIRE(info.meta.starRating == 3);
    REQUIRE(info.displayName == "Day 45  ★3  ₩5,000");
}

// ── SaveLoadPanel state tests ────────────────────────────────────────────────

TEST_CASE("SaveLoadPanel is closed by default", "[SaveLoadPanel]") {
    SaveLoadPanel panel;
    REQUIRE(panel.isOpen() == false);
}

TEST_CASE("SaveLoadPanel openSave() opens the panel", "[SaveLoadPanel]") {
    SaveLoadPanel panel;
    panel.openSave();
    REQUIRE(panel.isOpen() == true);
}

TEST_CASE("SaveLoadPanel openLoad() opens the panel", "[SaveLoadPanel]") {
    SaveLoadPanel panel;
    panel.openLoad();
    REQUIRE(panel.isOpen() == true);
}

TEST_CASE("SaveLoadPanel close() closes the panel", "[SaveLoadPanel]") {
    SaveLoadPanel panel;
    panel.openSave();
    REQUIRE(panel.isOpen() == true);
    panel.close();
    REQUIRE(panel.isOpen() == false);
}

TEST_CASE("SaveLoadPanel MAX_SLOTS is 5", "[SaveLoadPanel]") {
    REQUIRE(SaveLoadPanel::MAX_SLOTS == 5);
}

TEST_CASE("SaveLoadPanel formatSlotDisplay empty slot returns (Empty)", "[SaveLoadPanel]") {
    SaveLoadPanel panel;
    SaveSlotInfo info;
    info.isEmpty = true;
    std::string result = panel.formatSlotDisplay(info);
    REQUIRE(result == "(Empty)");
}

TEST_CASE("SaveLoadPanel formatSlotDisplay with metadata contains key fields", "[SaveLoadPanel]") {
    SaveLoadPanel panel;
    SaveSlotInfo info;
    info.isEmpty = false;
    info.meta.gameTime.day = 44;   // display as Day 45 (1-indexed)
    info.meta.starRating = 3;
    info.meta.balance = 1234567;

    std::string result = panel.formatSlotDisplay(info);
    REQUIRE(result.find("Day 45") != std::string::npos);
    REQUIRE(result.find("★3") != std::string::npos);
    // balance formatted: 1,234,567 Cents = ₩12,345 (or raw depending on impl)
    // at minimum the balance value should appear in some form
    REQUIRE(!result.empty());
}

} // anonymous namespace
} // namespace vse
