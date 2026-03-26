#include "renderer/SpriteSheet.h"
#include <SDL_image.h>
#include <spdlog/spdlog.h>
#include <cmath>

namespace vse {

// Frame colors matching current NPC rendering logic
// Frames: idle_0, idle_1, walk_0, walk_1, work_0, work_1, rest_0, elevator_0
const SpriteSheet::FrameColor SpriteSheet::frameColors_[8] = {
    // idle_0: bright cyan (Idle state)
    { 50, 250, 255, 0, 200, 220 },
    // idle_1: slightly brighter cyan
    { 60, 255, 255, 10, 210, 230 },
    // walk_0: bright cyan (Walking uses idle colors for now)
    { 50, 250, 255, 0, 200, 220 },
    // walk_1: slightly brighter cyan
    { 60, 255, 255, 10, 210, 230 },
    // work_0: bright green (Working state)
    { 50, 255, 150, 0, 220, 100 },
    // work_1: slightly brighter green
    { 60, 255, 160, 10, 230, 110 },
    // rest_0: bright yellow (Resting state)
    { 255, 255, 100, 255, 220, 50 },
    // elevator_0: bright purple (Elevator state)
    { 230, 150, 255, 180, 100, 255 }
};

SpriteSheet::SpriteSheet(SDL_Renderer* renderer, const std::string& path)
    : renderer_(renderer)
    , texture_(nullptr)
    , frameWidth_(16)
    , frameHeight_(32)
    , frameCount_(8)
{
    // Initialize SDL_image if not already done
    static bool sdl_image_initialized = false;
    if (!sdl_image_initialized) {
        int imgFlags = IMG_INIT_PNG;
        if ((IMG_Init(imgFlags) & imgFlags) != imgFlags) {
            spdlog::warn("SDL_image PNG initialization failed: {}", IMG_GetError());
        } else {
            sdl_image_initialized = true;
        }
    }
    
    if (sdl_image_initialized) {
        texture_ = IMG_LoadTexture(renderer_, path.c_str());
        if (texture_) {
            spdlog::info("SpriteSheet loaded: {}", path);
            
            // Get texture dimensions to verify
            int texW, texH;
            SDL_QueryTexture(texture_, nullptr, nullptr, &texW, &texH);
            
            // Verify dimensions match expected (128x32 for 8 frames of 16x32)
            if (texW == 128 && texH == 32) {
                spdlog::debug("SpriteSheet dimensions OK: {}x{}", texW, texH);
            } else {
                spdlog::warn("SpriteSheet dimensions unexpected: {}x{} (expected 128x32)", texW, texH);
                // Still usable, but frames might not align correctly
            }
        } else {
            spdlog::warn("Failed to load sprite sheet {}: {}. Using fallback rendering.", 
                        path, IMG_GetError());
        }
    } else {
        spdlog::warn("SDL_image not available. Using fallback rendering.");
    }
}

SpriteSheet::~SpriteSheet() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
}

void SpriteSheet::drawFrame(int frameIndex, float x, float y, float w, float h) {
    if (frameIndex < 0 || frameIndex >= frameCount_) {
        spdlog::warn("SpriteSheet::drawFrame: invalid frame index {}", frameIndex);
        frameIndex = 0;
    }
    
    if (texture_) {
        // Calculate source rectangle for the frame
        SDL_Rect srcRect = {
            frameIndex * frameWidth_,
            0,
            frameWidth_,
            frameHeight_
        };
        
        SDL_FRect dstRect = { x, y, w, h };
        SDL_RenderCopyF(renderer_, texture_, &srcRect, &dstRect);
    } else {
        // Fallback to colored rectangle
        drawFallback(frameIndex, x, y, w, h);
    }
}

void SpriteSheet::drawFallback(int frameIndex, float x, float y, float w, float h) {
    const FrameColor& color = frameColors_[frameIndex];
    
    // Head (top 30%)
    float headH = h * 0.30f;
    float headW = w * 0.75f;
    float headX = x + (w - headW) * 0.5f;
    
    SDL_SetRenderDrawColor(renderer_,
        std::min(255, static_cast<int>(color.headR)),
        std::min(255, static_cast<int>(color.headG)),
        std::min(255, static_cast<int>(color.headB)),
        230);
    SDL_FRect head = { headX, y, headW, headH };
    SDL_RenderFillRectF(renderer_, &head);
    
    // Body (bottom 70%)
    float bodyY = y + headH;
    SDL_SetRenderDrawColor(renderer_, 
        color.bodyR, color.bodyG, color.bodyB, 230);
    SDL_FRect body = { x, bodyY, w, h - headH };
    SDL_RenderFillRectF(renderer_, &body);
    
    // White outline
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 180);
    SDL_FRect outline = { x, y, w, h };
    SDL_RenderDrawRectF(renderer_, &outline);
}

} // namespace vse