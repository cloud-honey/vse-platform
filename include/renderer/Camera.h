#pragma once
#include "core/Types.h"

namespace vse {

/**
 * Camera — 월드 좌표 ↔ 화면 좌표 변환.
 *
 * 좌표계:
 * - 월드: 좌하단 원점, Y 위로 증가 (타일 좌표)
 * - 화면: 좌상단 원점, Y 아래로 증가 (SDL2 표준)
 *
 * 변환:
 *   screenX = (worldX - cameraX) * zoom
 *   screenY = viewportH - (worldY - cameraY) * zoom
 */
class Camera {
public:
    Camera() = default;
    Camera(int viewportW, int viewportH, int tileSize,
           float minZoom = 0.25f, float maxZoom = 4.0f);

    // ── 카메라 이동 ──────────────────
    void pan(float dx, float dy);       // 픽셀 단위 이동
    void zoom(float delta);             // 확대/축소 (delta: +면 확대, -면 축소)
    void zoomAt(float delta, float pivotScreenX, float pivotScreenY); // 마우스 피벗 줌
    void clampToWorld(float worldW, float worldH, float margin = 2.0f); // 월드 경계 클램프
    void reset();                       // 초기 위치/줌으로 복원
    void centerOn(float worldX, float worldY);

    // ── 좌표 변환 ────────────────────
    float worldToScreenX(float worldX) const;
    float worldToScreenY(float worldY) const;
    float screenToWorldX(float screenX) const;
    float screenToWorldY(float screenY) const;
    float tileToScreenX(int tileX) const;
    float tileToScreenY(int tileFloor) const;
    int   screenToTileX(float screenX) const;
    int   screenToTileFloor(float screenY) const;

    // ── Getters ──────────────────────
    float x()         const { return x_; }
    float y()         const { return y_; }
    float zoomLevel() const { return zoom_; }
    int   viewportW() const { return viewportW_; }
    int   viewportH() const { return viewportH_; }
    int   tileSize()  const { return tileSize_; }
    float minZoom()   const { return minZoom_; }
    float maxZoom()   const { return maxZoom_; }

    void setViewport(int w, int h) { viewportW_ = w; viewportH_ = h; }

private:
    float x_         = 0.0f;
    float y_         = 0.0f;
    float zoom_      = 1.0f;
    int   viewportW_ = 1280;
    int   viewportH_ = 720;
    int   tileSize_  = 32;
    float minZoom_   = 0.25f;
    float maxZoom_   = 4.0f;
};

} // namespace vse
