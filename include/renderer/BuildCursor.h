#pragma once
#include "renderer/BuildModeState.h"
#include "renderer/Camera.h"
#include <SDL.h>

namespace vse {

/**
 * BuildCursor — 건설 모드 시 마우스 커서 피드백을 그리는 클래스.
 *
 * 마우스가 호버링하는 타일에 반투명 오버레이를 그림:
 * - BuildFloor: 전체 층 너비 하이라이트 (파랑, alpha 100)
 * - PlaceTenant: tenantWidth 타일 하이라이트 (초록, alpha 100)
 * - None: 오버레이 없음
 */
class BuildCursor {
public:
    void draw(SDL_Renderer* r, const Camera& cam, int mouseX, int mouseY,
              const BuildModeState& mode, int tileSize);

private:
    void drawFloorHighlight(SDL_Renderer* r, const Camera& cam, int floor, int tileSize);
    void drawTenantHighlight(SDL_Renderer* r, const Camera& cam, int tileX, int floor,
                             int width, int tileSize);
};

} // namespace vse