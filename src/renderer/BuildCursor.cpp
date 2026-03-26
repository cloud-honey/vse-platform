#include "renderer/BuildCursor.h"

namespace vse {

void BuildCursor::draw(SDL_Renderer* r, const Camera& cam, int mouseX, int mouseY,
                       const BuildModeState& mode, int tileSize)
{
    if (!mode.active || mode.action == BuildAction::None) {
        return;
    }

    // 화면 좌표를 타일 좌표로 변환
    int tileX = cam.screenToTileX(mouseX);
    int floor = cam.screenToTileFloor(mouseY);

    // 타일 좌표가 유효한 범위인지 확인
    if (tileX < 0 || floor < 0) {
        return;
    }

    switch (mode.action) {
    case BuildAction::BuildFloor:
        drawFloorHighlight(r, cam, floor, tileSize);
        break;
    case BuildAction::PlaceTenant:
        drawTenantHighlight(r, cam, tileX, floor, mode.tenantWidth, tileSize);
        break;
    case BuildAction::None:
    default:
        break;
    }
}

void BuildCursor::drawFloorHighlight(SDL_Renderer* r, const Camera& cam, int floor, int tileSize)
{
    // 전체 층 너비 하이라이트 (파랑, alpha 100)
    SDL_SetRenderDrawColor(r, 0, 120, 255, 100);
    
    // 층의 모든 타일에 대해 하이라이트
    for (int x = 0; x < cam.viewportW() / tileSize + 2; ++x) {
        float screenX = cam.tileToScreenX(x);
        float screenY = cam.tileToScreenY(floor);
        
        SDL_Rect rect = {
            static_cast<int>(screenX),
            static_cast<int>(screenY),
            tileSize,
            tileSize
        };
        
        SDL_RenderFillRect(r, &rect);
    }
}

void BuildCursor::drawTenantHighlight(SDL_Renderer* r, const Camera& cam, int tileX, int floor,
                                      int width, int tileSize)
{
    // tenantWidth 타일 하이라이트 (초록, alpha 100)
    SDL_SetRenderDrawColor(r, 0, 255, 120, 100);
    
    // 시작 타일 위치 계산 (중앙 정렬)
    int startX = tileX - width / 2;
    
    for (int i = 0; i < width; ++i) {
        int currentX = startX + i;
        
        // 타일이 그리드 내에 있는지 확인
        if (currentX < 0) continue;
        
        float screenX = cam.tileToScreenX(currentX);
        float screenY = cam.tileToScreenY(floor);
        
        SDL_Rect rect = {
            static_cast<int>(screenX),
            static_cast<int>(screenY),
            tileSize,
            tileSize
        };
        
        SDL_RenderFillRect(r, &rect);
    }
}

} // namespace vse