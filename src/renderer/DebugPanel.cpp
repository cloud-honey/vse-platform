#include "renderer/DebugPanel.h"
#include "imgui.h"

namespace vse {

void DebugPanel::draw(const RenderFrame& frame) const
{
    if (!visible_) return;

    const auto& d = frame.debug;

    // 첫 실행 시 위치 지정, 이후 드래그 이동 가능
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(220, 190), ImGuiCond_FirstUseEver);
    ImGui::Begin("VSE Debug", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings);

    // 시뮬레이션 시간 (gameDay 0-indexed → UI 표시 +1)
    ImGui::Text("Day %d  %02d:%02d", d.gameDay + 1, d.gameHour, d.gameMinute);
    ImGui::Text("Tick: %d", d.gameTick);
    ImGui::Text("Speed: %.1fx%s", d.simSpeed, d.isPaused ? " [PAUSED]" : "");
    ImGui::Separator();

    // NPC 상태
    ImGui::Text("NPC  total:%d  idle:%d  work:%d  rest:%d",
                d.npcTotal, d.npcIdle, d.npcWorking, d.npcResting);
    ImGui::Text("Satisfaction: %.1f%%", d.avgSatisfaction);
    ImGui::Separator();

    // 렌더 성능
    ImGui::Text("FPS: %.1f  Elevators: %d", d.fps, d.elevatorCount);

    ImGui::End();
}

} // namespace vse
