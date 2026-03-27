#include "renderer/SaveLoadPanel.h"
#include "imgui.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <climits>

namespace vse {

void SaveLoadPanel::openSave() {
    open_ = true;
    saveMode_ = true;
    pendingOverwriteSlot_ = -1;
}

void SaveLoadPanel::openLoad() {
    open_ = true;
    saveMode_ = false;
    pendingOverwriteSlot_ = -1;
}

void SaveLoadPanel::close() {
    open_ = false;
    pendingOverwriteSlot_ = -1;
}

void SaveLoadPanel::draw(const std::vector<SaveSlotInfo>& slotInfos) {
    if (!open_) return;

    const char* title = saveMode_ ? "Save Game" : "Load Game";
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin(title, &open_)) {
        drawSlotList(slotInfos);
        
        // Cancel button
        if (ImGui::Button("Cancel")) {
            close();
        }
        
        drawOverwriteConfirm();
    }
    ImGui::End();
    
    // If window was closed via X button
    if (!open_) {
        close();
    }
}

void SaveLoadPanel::drawSlotList(const std::vector<SaveSlotInfo>& slotInfos) {
    ImGui::Text("Select a save slot:");
    ImGui::Separator();
    
    for (const auto& slot : slotInfos) {
        if (slot.slotIndex >= MAX_SLOTS) continue;
        
        ImGui::PushID(slot.slotIndex);
        
        // Slot header
        std::string slotLabel;
        if (slot.slotIndex == 0) {
            slotLabel = "Slot 0 (Auto-save)";
        } else {
            slotLabel = "Slot " + std::to_string(slot.slotIndex);
        }
        
        if (ImGui::CollapsingHeader(slotLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            // Slot content
            std::string displayText = formatSlotDisplay(slot);
            ImGui::Text("%s", displayText.c_str());
            
            // Action buttons
            if (saveMode_) {
                // Save mode
                if (slot.isEmpty) {
                    if (ImGui::Button("Save")) {
                        if (onSaveRequested) {
                            onSaveRequested(slot.slotIndex);
                            close();
                        }
                    }
                } else {
                    if (ImGui::Button("Overwrite")) {
                        pendingOverwriteSlot_ = slot.slotIndex;
                        ImGui::OpenPopup("Overwrite?##SaveLoad");
                    }
                }
            } else {
                // Load mode
                if (!slot.isEmpty) {
                    if (ImGui::Button("Load")) {
                        if (onLoadRequested) {
                            onLoadRequested(slot.slotIndex);
                            close();
                        }
                    }
                } else {
                    ImGui::TextDisabled("(Empty slot)");
                }
            }
        }
        
        ImGui::PopID();
    }
}

void SaveLoadPanel::drawOverwriteConfirm() {
    if (pendingOverwriteSlot_ < 0) return;
    
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Overwrite?##SaveLoad", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to overwrite slot %d?", pendingOverwriteSlot_);
        ImGui::Separator();
        
        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            if (onSaveRequested) {
                onSaveRequested(pendingOverwriteSlot_);
            }
            pendingOverwriteSlot_ = -1;
            ImGui::CloseCurrentPopup();
            close();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0))) {
            pendingOverwriteSlot_ = -1;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

std::string SaveLoadPanel::formatSlotDisplay(const SaveSlotInfo& info) const {
    if (info.isEmpty) {
        return "(Empty)";
    }
    
    std::stringstream ss;
    
    // Day (1-indexed for display)
    ss << "Day " << (info.meta.gameTime.day + 1);
    
    // Star rating
    int stars = info.meta.starRating;
    if (stars >= 1 && stars <= 5) {
        ss << "  ★" << stars;
    }
    
    // Balance with thousand separator
    int64_t balance = info.meta.balance;
    bool negative = (balance < 0);
    uint64_t val = negative
        ? (balance == INT64_MIN
            ? static_cast<uint64_t>(INT64_MAX) + 1ULL
            : static_cast<uint64_t>(-balance))
        : static_cast<uint64_t>(balance);
    
    std::string digits = std::to_string(val);
    std::string formatted;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) formatted = "," + formatted;
        formatted = digits[i] + formatted;
        ++count;
    }
    
    ss << "  ₩" << (negative ? "-" : "") << formatted;
    
    // Building name (if not empty)
    if (!info.meta.buildingName.empty()) {
        ss << "\n" << info.meta.buildingName;
    }
    
    // Playtime
    uint64_t hours = info.meta.playtimeSeconds / 3600;
    uint64_t minutes = (info.meta.playtimeSeconds % 3600) / 60;
    if (hours > 0) {
        ss << "\nPlaytime: " << hours << "h " << minutes << "m";
    } else if (minutes > 0) {
        ss << "\nPlaytime: " << minutes << "m";
    }
    
    return ss.str();
}

} // namespace vse