#include "renderer/Camera.h"
#include <algorithm>
#include <cmath>

namespace vse {

Camera::Camera(int viewportW, int viewportH, int tileSize,
               float minZoom, float maxZoom)
    : viewportW_(viewportW)
    , viewportH_(viewportH)
    , tileSize_(tileSize)
    , minZoom_(minZoom)
    , maxZoom_(maxZoom)
{}

void Camera::pan(float dx, float dy)
{
    x_ -= dx / zoom_;
    y_ += dy / zoom_;
}

void Camera::zoom(float delta)
{
    zoom_ = std::clamp(zoom_ + delta, minZoom_, maxZoom_);
}

void Camera::zoomAt(float delta, float pivotScreenX, float pivotScreenY)
{
    // World position under pivot BEFORE zoom
    float worldX = screenToWorldX(pivotScreenX);
    float worldY = screenToWorldY(pivotScreenY);
    
    // Apply zoom
    zoom_ = std::clamp(zoom_ + delta, minZoom_, maxZoom_);
    
    // Adjust camera offset so same world point stays under pivot
    x_ = worldX - pivotScreenX / zoom_;
    y_ = worldY - (viewportH_ - pivotScreenY) / zoom_;
}

void Camera::clampToWorld(float worldW, float worldH, float margin)
{
    float visW = viewportW_ / zoom_;
    float visH = viewportH_ / zoom_;
    
    // Clamp camera position within world boundaries with margin
    x_ = std::clamp(x_, -margin, worldW - visW + margin);
    y_ = std::clamp(y_, -margin, worldH - visH + margin);
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

float Camera::worldToScreenX(float worldX) const
{
    return (worldX - x_) * zoom_;
}

float Camera::worldToScreenY(float worldY) const
{
    return viewportH_ - (worldY - y_) * zoom_;
}

float Camera::screenToWorldX(float screenX) const
{
    return screenX / zoom_ + x_;
}

float Camera::screenToWorldY(float screenY) const
{
    return (viewportH_ - screenY) / zoom_ + y_;
}

float Camera::tileToScreenX(int tileX) const
{
    float worldX = static_cast<float>(tileX * tileSize_);
    return worldToScreenX(worldX);
}

float Camera::tileToScreenY(int tileFloor) const
{
    float worldY = static_cast<float>((tileFloor + 1) * tileSize_);
    return worldToScreenY(worldY);
}

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
