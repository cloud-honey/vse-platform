#include "renderer/HUDPanel.h"
#include "imgui.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <climits>
#include <algorithm>

namespace vse {

void HUDPanel::draw(const RenderFrame& frame)
{
    if (!visible_ || !frame.showHUD) return;

    // Update toast timers
    float currentTime = ImGui::GetTime();
    float dt = 0.0f;
    if (lastFrameTime_ > 0.0f) {
        dt = currentTime - lastFrameTime_;
        if (dt > 0.1f) dt = 0.1f; // Clamp large jumps
    }
    lastFrameTime_ = currentTime;
    updateToasts(dt);

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

    // Row 1: Balance with daily change indicator
    std::string balanceStr = formatBalance(frame.balance);
    std::string changeStr = formatDailyChange(frame.dailyChange);
    ImVec4 balanceColor = (frame.balance >= 0) 
        ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)  // Green for positive
        : ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red for negative
    ImVec4 changeColor = (frame.dailyChange >= 0)
        ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)  // Green for positive change
        : ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red for negative change
    
    ImGui::TextColored(balanceColor, "%s", balanceStr.c_str());
    if (!changeStr.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(changeColor, " %s", changeStr.c_str());
    }

    // Row 2: Daily Income/Expense
    std::string incomeStr = formatBalance(frame.dailyIncome);
    std::string expenseStr = formatBalance(frame.dailyExpense);
    ImGui::Text("In: %s | Out: %s", incomeStr.c_str(), expenseStr.c_str());

    // Row 3: Star rating (icons only, no text)
    std::string starsStr = formatStars(frame.starRating);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", starsStr.c_str());

    // Row 4: Current tick
    ImGui::Text("Tick:%d", frame.currentTick);

    // Row 5: Tenants/NPCs
    ImGui::Text("Tenants:%d NPC:%d", frame.tenantCount, frame.npcCount);

    ImGui::End();

    // Draw toasts
    drawToasts();
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
    
    return ss.str();
}

void HUDPanel::pushToast(const std::string& text, ToastMessage::Type type)
{
    // Remove oldest if at MAX_TOASTS capacity
    while (toasts_.size() >= MAX_TOASTS) toasts_.erase(toasts_.begin());
    toasts_.push_back({text, TOAST_DURATION, type});
}

void HUDPanel::updateToasts(float dt)
{
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
        [](const ToastMessage& t) { return t.remainingSeconds <= 0.0f; }),
        toasts_.end());
    for (auto& t : toasts_) t.remainingSeconds -= dt;
}

void HUDPanel::drawToasts()
{
    const float PADDING = 10.0f;
    const float TOAST_HEIGHT = 50.0f;
    const float TOAST_WIDTH = 300.0f;
    
    for (size_t i = 0; i < toasts_.size(); ++i) {
        const auto& toast = toasts_[i];
        
        // Position: bottom-right, stacked upward
        float yPos = ImGui::GetIO().DisplaySize.y - PADDING - (i + 1) * (TOAST_HEIGHT + PADDING);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - PADDING - TOAST_WIDTH, yPos), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(TOAST_WIDTH, TOAST_HEIGHT));
        
        // Window flags
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav;
        
        // Color based on type
        ImVec4 bgColor;
        switch (toast.type) {
            case ToastMessage::Type::Info:
                bgColor = ImVec4(0.2f, 0.2f, 0.2f, 0.9f); // Dark gray
                break;
            case ToastMessage::Type::Warning:
                bgColor = ImVec4(0.8f, 0.6f, 0.0f, 0.9f); // Yellow
                break;
            case ToastMessage::Type::Error:
                bgColor = ImVec4(0.8f, 0.2f, 0.0f, 0.9f); // Red
                break;
        }
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
        ImGui::Begin(("Toast_" + std::to_string(i)).c_str(), nullptr, flags);
        
        // Progress bar for remaining time
        float progress = toast.remainingSeconds / TOAST_DURATION;
        ImGui::ProgressBar(progress, ImVec2(-1, 4));
        
        // Toast text
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        ImGui::TextWrapped("%s", toast.text.c_str());
        
        ImGui::End();
        ImGui::PopStyleColor();
    }
}

int HUDPanel::drawSpeedButtons(int currentSpeed)
{
    const float BUTTON_WIDTH = 60.0f;
    const float BUTTON_HEIGHT = 30.0f;
    const float PADDING = 10.0f;
    
    // Position: top-right corner
    float xPos = ImGui::GetIO().DisplaySize.x - PADDING - 3 * (BUTTON_WIDTH + PADDING);
    float yPos = PADDING;
    
    ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(3 * BUTTON_WIDTH + 2 * PADDING, BUTTON_HEIGHT + 2 * PADDING));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent
    ImGui::Begin("SpeedButtons", nullptr, flags);
    
    int clickedSpeed = -1;
    
    // ×1 button
    if (currentSpeed == 1) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f)); // Green for active
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
    }
    if (ImGui::Button("×1", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
        clickedSpeed = 1;
    }
    if (currentSpeed == 1) {
        ImGui::PopStyleColor(2);
    }
    
    ImGui::SameLine();
    
    // ×2 button
    if (currentSpeed == 2) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f)); // Green for active
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
    }
    if (ImGui::Button("×2", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
        clickedSpeed = 2;
    }
    if (currentSpeed == 2) {
        ImGui::PopStyleColor(2);
    }
    
    ImGui::SameLine();
    
    // ×3 button
    if (currentSpeed == 3) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f)); // Green for active
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
    }
    if (ImGui::Button("×3", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
        clickedSpeed = 3;
    }
    if (currentSpeed == 3) {
        ImGui::PopStyleColor(2);
    }
    
    ImGui::End();
    ImGui::PopStyleColor();
    
    return clickedSpeed;
}

int HUDPanel::drawToolbar(int viewportH)
{
    const float BUTTON_WIDTH = 100.0f;
    const float BUTTON_HEIGHT = 40.0f;
    const float PADDING = 10.0f;
    const float TOOLBAR_HEIGHT = BUTTON_HEIGHT + 2 * PADDING;
    
    // Position: bottom center
    float totalWidth = 4 * BUTTON_WIDTH + 3 * PADDING;
    float xPos = (ImGui::GetIO().DisplaySize.x - totalWidth) / 2.0f;
    float yPos = viewportH - TOOLBAR_HEIGHT;
    
    ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(totalWidth, TOOLBAR_HEIGHT));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.8f)); // Semi-transparent dark
    ImGui::Begin("ConstructionToolbar", nullptr, flags);
    
    int clickedAction = 0; // 0=none, 1=floor, 2=office, 3=residential, 4=commercial
    
    // Floor button
    if (ImGui::Button("🏢 Floor", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
        clickedAction = 1;
    }
    
    ImGui::SameLine();
    
    // Office button
    if (ImGui::Button("🏢 Office", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
        clickedAction = 2;
    }
    
    ImGui::SameLine();
    
    // Residential button
    if (ImGui::Button("🏠 Residential", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
        clickedAction = 3;
    }
    
    ImGui::SameLine();
    
    // Commercial button
    if (ImGui::Button("🏪 Commercial", ImVec2(BUTTON_WIDTH, BUTTON_HEIGHT))) {
        clickedAction = 4;
    }
    
    ImGui::End();
    ImGui::PopStyleColor();
    
    return clickedAction;
}

std::string HUDPanel::formatDailyChange(int64_t change) const
{
    if (change == 0) {
        return "";
    }
    
    std::string sign = (change > 0) ? "↑" : "↓";
    
    // Format the absolute value
    bool negative = (change < 0);
    uint64_t val = negative ? static_cast<uint64_t>(-change) : static_cast<uint64_t>(change);
    
    // Build digits in groups of 3 from right
    std::string digits = std::to_string(val);
    std::string result;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) result = "," + result;
        result = digits[i] + result;
        ++count;
    }
    
    return sign + "₩" + result;
}

} // namespace vse