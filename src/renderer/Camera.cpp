#include "renderer/Camera.h"
#include <algorithm>
#include <cmath>

namespace vse {

Camera::Camera(int viewportW, int viewportH, int tileSize)
    : viewportW_(viewportW)
    , viewportH_(viewportH)
    , tileSize_(tileSize)
{}

void Camera::pan(float dx, float dy)
{
    // dx, dy는 화면 픽셀 → 줌 반영하여 월드 이동
    x_ -= dx / zoom_;
    y_ += dy / zoom_;   // 화면 아래로 드래그 = 월드 위로 이동 → +y
}

void Camera::zoom(float delta)
{
    zoom_ = std::clamp(zoom_ + delta, MIN_ZOOM, MAX_ZOOM);
}

void Camera::reset()
{
    x_    = 0.0f;
    y_    = 0.0f;
    zoom_ = 1.0f;
}

void Camera::centerOn(float worldX, float worldY)
{
    x_ = worldX - (viewportW_ / (2.0f * zoom_));
    y_ = worldY - (viewportH_ / (2.0f * zoom_));
}

// ── 월드(px) → 화면(px) ──────────────────────────────────
float Camera::worldToScreenX(float worldX) const
{
    return (worldX - x_) * zoom_;
}

float Camera::worldToScreenY(float worldY) const
{
    // 월드 Y 위로 증가 → 화면 Y 아래로 증가 (반전)
    return viewportH_ - (worldY - y_) * zoom_;
}

// ── 화면(px) → 월드(px) ──────────────────────────────────
float Camera::screenToWorldX(float screenX) const
{
    return screenX / zoom_ + x_;
}

float Camera::screenToWorldY(float screenY) const
{
    return (viewportH_ - screenY) / zoom_ + y_;
}

// ── 타일 좌표 → 화면(px) ─────────────────────────────────
float Camera::tileToScreenX(int tileX) const
{
    float worldX = static_cast<float>(tileX * tileSize_);
    return worldToScreenX(worldX);
}

float Camera::tileToScreenY(int tileFloor) const
{
    // 타일 상단 = (floor+1) * tileSize
    float worldY = static_cast<float>((tileFloor + 1) * tileSize_);
    return worldToScreenY(worldY);
}

// ── 화면(px) → 타일 좌표 ─────────────────────────────────
int Camera::screenToTileX(float screenX) const
{
    float worldX = screenToWorldX(screenX);
    return static_cast<int>(std::floor(worldX / tileSize_));
}

int Camera::screenToTileFloor(float screenY) const
{
    float worldY = screenToWorldY(screenY);
    return static_cast<int>(std::floor(worldY / tileSize_));
}

} // namespace vse
