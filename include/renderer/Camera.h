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
    Camera(int viewportW, int viewportH, int tileSize);

    // ── 카메라 이동 ──────────────────
    void pan(float dx, float dy);       // 픽셀 단위 이동
    void zoom(float delta);             // 확대/축소 (delta: +면 확대, -면 축소)
    void reset();                       // 초기 위치/줌으로 복원
    void centerOn(float worldX, float worldY);

    // ── 좌표 변환 ────────────────────
    // 월드 좌표(px) → 화면 좌표(px)
    float worldToScreenX(float worldX) const;
    float worldToScreenY(float worldY) const;

    // 화면 좌표(px) → 월드 좌표(px)
    float screenToWorldX(float screenX) const;
    float screenToWorldY(float screenY) const;

    // 타일 좌표 → 화면 좌표(px)
    float tileToScreenX(int tileX) const;
    float tileToScreenY(int tileFloor) const;

    // 화면 좌표(px) → 타일 좌표
    int screenToTileX(float screenX) const;
    int screenToTileFloor(float screenY) const;

    // ── Getters ──────────────────────
    float x()         const { return x_; }
    float y()         const { return y_; }
    float zoomLevel() const { return zoom_; }
    int   viewportW() const { return viewportW_; }
    int   viewportH() const { return viewportH_; }
    int   tileSize()  const { return tileSize_; }

    void setViewport(int w, int h) { viewportW_ = w; viewportH_ = h; }

private:
    float x_         = 0.0f;   // 카메라 좌하단 월드X (px)
    float y_         = 0.0f;   // 카메라 좌하단 월드Y (px)
    float zoom_      = 1.0f;
    int   viewportW_ = 1280;
    int   viewportH_ = 720;
    int   tileSize_  = 32;

    static constexpr float MIN_ZOOM = 0.25f;
    static constexpr float MAX_ZOOM = 4.0f;
};

} // namespace vse
