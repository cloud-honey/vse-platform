#include "renderer/BuildCursor.h"
#include "imgui.h"
#include <string>
#include <sstream>
#include <iomanip>

namespace vse {

void BuildCursor::drawOverlay(SDL_Renderer* r, const Camera& cam, int mouseX, int mouseY,
                              const BuildModeState& mode, int tileSize)
{
    // SDL-only rendering: tile highlight. No ImGui calls here.
    if (!mode.active || mode.action == BuildAction::None) {
        return;
    }

    // Convert screen coordinates to tile coordinates
    int tileX = cam.screenToTileX(mouseX);
    int floor = cam.screenToTileFloor(mouseY);

    // Skip rendering if tile is out of bounds
    if (tileX < 0 || floor < 0) {
        return;
    }

    switch (mode.action) {
    case BuildAction::BuildFloor:
        drawFloorHighlight(r, cam, floor, tileSize, mode.isValidPlacement);
        break;
    case BuildAction::PlaceTenant:
        drawTenantHighlight(r, cam, tileX, floor, mode.tenantWidth, tileSize, mode.isValidPlacement);
        break;
    case BuildAction::None:
    default:
        break;
    }
}

void BuildCursor::drawImGui(int mouseX, int mouseY, const BuildModeState& mode)
{
    // ImGui-only rendering: tooltip. Must be called inside ImGui::NewFrame()...ImGui::Render().
    if (!mode.active || mode.action == BuildAction::None) {
        return;
    }

    // Show tooltip when there is a cost to display or placement is invalid
    if (mode.previewCost > 0 || !mode.isValidPlacement) {
        drawCostTooltip(mouseX, mouseY, mode.previewCost, mode.isValidPlacement);
    }
}

void BuildCursor::draw(SDL_Renderer* r, const Camera& cam, int mouseX, int mouseY,
                       const BuildModeState& mode, int tileSize)
{
    // Legacy combined call — SDL overlay only (no ImGui). Use drawOverlay+drawImGui instead.
    drawOverlay(r, cam, mouseX, mouseY, mode, tileSize);
}

void BuildCursor::drawFloorHighlight(SDL_Renderer* r, const Camera& cam, int floor,
                                     int tileSize, bool isValid)
{
    // Color based on validity: green when valid, red when invalid
    if (isValid) {
        SDL_SetRenderDrawColor(r, 0, 180, 80, 120);  // Green for valid
    } else {
        SDL_SetRenderDrawColor(r, 220, 50, 50, 120); // Red for invalid
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    float zoom = cam.zoomLevel();
    float scaledTile = tileSize * zoom;
    // zoom 적용된 타일 크기 기준으로 가시 타일 수 계산
    int visibleTiles = static_cast<int>(cam.viewportW() / scaledTile) + 2;

    float screenY = cam.tileToScreenY(floor);

    for (int x = 0; x < visibleTiles; ++x) {
        float screenX = cam.tileToScreenX(x);
        if (screenX + scaledTile < 0 || screenX > cam.viewportW()) continue;

        SDL_FRect rect = {screenX, screenY, scaledTile, scaledTile};
        SDL_RenderFillRectF(r, &rect);
    }
}

void BuildCursor::drawTenantHighlight(SDL_Renderer* r, const Camera& cam, int tileX, int floor,
                                      int width, int tileSize, bool isValid)
{
    // Color based on validity: green when valid, red when invalid
    if (isValid) {
        SDL_SetRenderDrawColor(r, 0, 180, 80, 120);  // Green for valid
    } else {
        SDL_SetRenderDrawColor(r, 220, 50, 50, 120); // Red for invalid
    }
    
    // 시작 타일 위치 계산 (중앙 정렬)
    int startX = tileX - width / 2;
    
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    float zoom = cam.zoomLevel();
    float scaledTile = tileSize * zoom;
    float screenY = cam.tileToScreenY(floor);

    for (int i = 0; i < width; ++i) {
        int currentX = startX + i;
        if (currentX < 0) continue;

        float screenX = cam.tileToScreenX(currentX);
        if (screenX + scaledTile < 0 || screenX > cam.viewportW()) continue;

        SDL_FRect rect = {screenX, screenY, scaledTile, scaledTile};
        SDL_RenderFillRectF(r, &rect);
    }
}

void BuildCursor::drawCostTooltip(int mouseX, int mouseY, int64_t previewCost, bool isValid)
{
    // Set tooltip position near cursor
    ImGui::SetNextWindowPos(ImVec2(mouseX + 20, mouseY + 20), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.8f);
    
    ImGui::BeginTooltip();
    
    if (!isValid) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Cannot build here");
    } else if (previewCost > 0) {
        // Format cost with thousand separator
        std::stringstream ss;
        ss.imbue(std::locale(""));
        ss << "₩" << std::fixed << std::setprecision(0) << (previewCost / 100.0);
        ImGui::Text("%s", ss.str().c_str());
    }
    
    ImGui::EndTooltip();
}

bool BuildCursor::drawTenantSelectPopup(int& outTenantType)
{
    bool selected = false;
    
    if (popupOpen_) {
        ImGui::OpenPopup("Select Tenant Type");
        popupOpen_ = false;
    }
    
    // Center the popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Select Tenant Type", nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        
        ImGui::Text("Choose tenant type to build:");
        ImGui::Separator();
        
        if (ImGui::Button("Office (₩5,000)")) {
            outTenantType = 0; // Office
            selected = true;
            ImGui::CloseCurrentPopup();
        }
        
        if (ImGui::Button("Residential (₩3,000)")) {
            outTenantType = 1; // Residential
            selected = true;
            ImGui::CloseCurrentPopup();
        }
        
        if (ImGui::Button("Commercial (₩8,000)")) {
            outTenantType = 2; // Commercial
            selected = true;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    return selected;
}

void BuildCursor::openTenantSelectPopup()
{
    popupOpen_ = true;
}

} // namespace vse