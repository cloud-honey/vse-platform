#include "renderer/HUDPanel.h"
#include "imgui.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>

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
        ImGuiWindowFlags_NoNav);

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

std::string HUDPanel::formatBalance(int64_t balance) const
{
    std::stringstream ss;
    
    // Add Korean Won symbol
    ss << "₩";
    
    // Handle negative balance
    if (balance < 0) {
        ss << "-";
        balance = -balance;
    }
    
    // Format with thousands separators
    if (balance >= 1000000) {
        ss << (balance / 1000000) << ",";
        balance %= 1000000;
        ss << std::setw(6) << std::setfill('0') << balance;
    } else if (balance >= 1000) {
        ss << (balance / 1000) << ",";
        balance %= 1000;
        ss << std::setw(3) << std::setfill('0') << balance;
    } else {
        ss << balance;
    }
    
    return ss.str();
}

std::string HUDPanel::formatStars(float rating) const
{
    // Clamp rating to 0.0-5.0 range
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