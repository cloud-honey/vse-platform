#include "renderer/HUDPanel.h"
#include "imgui.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <climits>

namespace vse {

void HUDPanel::draw(const RenderFrame& frame)
{
    if (!visible_ || !frame.showHUD) return;

    // HUD 위치: top-left corner, fixed position
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f); // Semi-transparent background
    
    // Window flags: NoTitleBar, NoResize, NoMove, AlwaysAutoResize
    ImGui::Begin("Tower Tycoon HUD", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs |       // 클릭 인터랙션 방해 방지 (TASK-03-006 대비)
        ImGuiWindowFlags_NoMouseInputs);  // 마우스 이벤트 통과

    // Title
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Tower Tycoon");
    ImGui::Separator();

    // Row 1: Balance
    std::string balanceStr = formatBalance(frame.balance);
    ImVec4 balanceColor = (frame.balance >= 0) 
        ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)  // Green for positive
        : ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red for negative
    ImGui::TextColored(balanceColor, "%s", balanceStr.c_str());

    // Row 2: Star rating
    std::string starsStr = formatStars(frame.starRating);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", starsStr.c_str());

    // Row 3: Current tick
    ImGui::Text("T:%d", frame.currentTick);

    // Row 4: Tenants/NPCs
    ImGui::Text("T:%d NPC:%d", frame.tenantCount, frame.npcCount);

    ImGui::End();
}

std::string HUDPanel::formatBalance(int64_t balance)
{
    bool negative = (balance < 0);
    // INT64_MIN 안전 처리: -INT64_MIN은 uint64_t 최대값
    uint64_t val = negative
        ? (balance == INT64_MIN
            ? static_cast<uint64_t>(INT64_MAX) + 1ULL
            : static_cast<uint64_t>(-balance))
        : static_cast<uint64_t>(balance);

    // Build digits in groups of 3 from right
    std::string digits = std::to_string(val);
    std::string result;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) result = "," + result;
        result = digits[i] + result;
        ++count;
    }

    return std::string("₩") + (negative ? "-" : "") + result;
}

std::string HUDPanel::formatStars(float rating)
{
    // NaN/Inf 보호 + 0.0~5.0 클램프
    if (!std::isfinite(rating)) rating = 0.0f;
    rating = std::max(0.0f, std::min(5.0f, rating));
    
    std::stringstream ss;
    
    // Full stars
    int fullStars = static_cast<int>(rating);
    for (int i = 0; i < fullStars; ++i) {
        ss << "★";
    }
    
    // Empty stars
    int emptyStars = 5 - fullStars;
    for (int i = 0; i < emptyStars; ++i) {
        ss << "☆";
    }
    
    // Add numeric rating in parentheses
    ss << " (" << std::fixed << std::setprecision(1) << rating << ")";
    
    return ss.str();
}

} // namespace vse