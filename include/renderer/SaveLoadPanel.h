#pragma once
#include "core/IRenderCommand.h"
#include <vector>
#include <string>
#include <functional>

namespace vse {

/**
 * SaveLoadPanel — ImGui save/load slot UI.
 *
 * Displays up to MAX_SLOTS save slots with metadata.
 * Slot 0 is reserved for auto-save.
 * Calls callbacks on save/load actions — Layer 3 must not call domain directly.
 */
class SaveLoadPanel {
public:
    static constexpr int MAX_SLOTS = 5;  ///< Slots 0 (auto) + 1-4 (manual)

    /// Callbacks (set by Bootstrapper)
    std::function<void(int slotIndex)> onSaveRequested;   ///< User confirmed save
    std::function<void(int slotIndex)> onLoadRequested;   ///< User confirmed load

    /// Open save mode (call from pause menu "Save" button)
    void openSave();
    /// Open load mode (call from main menu "Load Game" button)
    void openLoad();
    /// Close panel
    void close();
    bool isOpen() const { return open_; }

    /// Draw the panel (call inside ImGui frame).
    /// slotInfos: metadata for each slot (empty SaveMetadata if slot is empty).
    void draw(const std::vector<SaveSlotInfo>& slotInfos);

    /// Format slot metadata for display string. Public for testability.
    std::string formatSlotDisplay(const SaveSlotInfo& info) const;

private:
    bool open_     = false;
    bool saveMode_ = true;   ///< true = save UI, false = load UI

    int  pendingOverwriteSlot_ = -1;  ///< Slot waiting for overwrite confirmation

    void drawSlotList(const std::vector<SaveSlotInfo>& slotInfos);
    void drawOverwriteConfirm();
};

} // namespace vse