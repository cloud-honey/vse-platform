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
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 120, 255, 100);

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
                                      int width, int tileSize)
{
    // tenantWidth 타일 하이라이트 (초록, alpha 100)
    SDL_SetRenderDrawColor(r, 0, 255, 120, 100);
    
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

} // namespace vse