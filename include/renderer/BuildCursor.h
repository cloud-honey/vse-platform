#pragma once
#include "renderer/BuildModeState.h"
#include "renderer/Camera.h"
#include <SDL.h>

namespace vse {

/**
 * BuildCursor — 건설 모드 시 마우스 커서 피드백을 그리는 클래스.
 *
 * Draws a semi-transparent overlay on the hovered tile(s) based on build mode:
 * - BuildFloor: full floor width highlight (green=valid, red=invalid)
 * - PlaceTenant: tenantWidth tiles highlight, center-aligned and left-clamped (green=valid, red=invalid)
 * - None: no overlay
 *
 * 렌더링 순서 필수:
 * 1. drawOverlay() — SDL BeginRender/EndRender 사이, ImGui::NewFrame() 전
 * 2. drawImGui()   — ImGui::NewFrame() 이후, ImGui::Render() 전
 */
class BuildCursor {
public:
    /// Draw SDL-only cursor overlay (highlight tiles). Call BEFORE ImGui::NewFrame().
    void drawOverlay(SDL_Renderer* r, const Camera& cam, int mouseX, int mouseY,
                     const BuildModeState& mode, int tileSize);

    /// Draw ImGui tooltip. Call AFTER ImGui::NewFrame(), BEFORE ImGui::Render().
    void drawImGui(int mouseX, int mouseY, const BuildModeState& mode);

    /// @deprecated Use drawOverlay() + drawImGui() separately. Kept for compatibility.
    void draw(SDL_Renderer* r, const Camera& cam, int mouseX, int mouseY,
              const BuildModeState& mode, int tileSize);

    /// Draw ImGui tenant selection popup (call after ImGui::NewFrame()).
    /// Returns true if a tenant type was selected; outTenantType is set.
    bool drawTenantSelectPopup(int& outTenantType);

    /// Open the tenant selection popup.
    void openTenantSelectPopup();

private:
    bool popupOpen_ = false;  ///< Pending popup open flag

    void drawFloorHighlight(SDL_Renderer* r, const Camera& cam, int floor,
                            int tileSize, bool isValid);
    void drawTenantHighlight(SDL_Renderer* r, const Camera& cam, int tileX, int floor,
                             int width, int tileSize, bool isValid);
    void drawCostTooltip(int mouseX, int mouseY, int64_t previewCost, bool isValid);
};

} // namespace vse