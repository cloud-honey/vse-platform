#include "renderer/ElevatorRenderer.h"
#include <spdlog/spdlog.h>

namespace vse {

ElevatorRenderer::ElevatorRenderer(SDL_Renderer* renderer)
    : renderer_(renderer)
{
}

ElevatorRenderer::~ElevatorRenderer()
{
    freeTextures();
}

bool ElevatorRenderer::loadSprites(const std::string& spritesDir)
{
    freeTextures();  // 재호출 시 기존 텍스처 누수 방지
    bool anyLoaded = false;
    
    texCar_ = loadTexture(spritesDir + "/elevator_car.png");
    if (texCar_) anyLoaded = true;
    
    texOpen_ = loadTexture(spritesDir + "/elevator_open.png");
    if (texOpen_) anyLoaded = true;
    
    if (!anyLoaded) {
        spdlog::warn("ElevatorRenderer: No elevator sprites loaded, will use color fallback");
    } else {
        spdlog::info("ElevatorRenderer: Loaded elevator sprites from {}", spritesDir);
    }
    
    return anyLoaded;
}

void ElevatorRenderer::drawElevator(const RenderElevator& elev, float sx, float sy, float tilePx)
{
    SDL_Texture* texture = nullptr;
    
    // Select texture based on elevator state
    if (elev.state == ElevatorState::Boarding || elev.state == ElevatorState::DoorOpening) {
        texture = texOpen_;
    } else {
        texture = texCar_;
    }
    
    if (texture) {
        // Draw sprite
        SDL_FRect dest = {sx, sy, tilePx, tilePx};
        SDL_RenderCopyF(renderer_, texture, nullptr, &dest);
        
        // Draw door state indicator (bright border) for boarding/opening states
        if (elev.state == ElevatorState::Boarding || elev.state == ElevatorState::DoorOpening) {
            SDL_SetRenderDrawColor(renderer_, 255, 255, 200, 180);
            SDL_RenderDrawRectF(renderer_, &dest);
        }
    } else {
        // Color fallback
        SDL_SetRenderDrawColor(renderer_, elev.color.r, elev.color.g,
                                elev.color.b, elev.color.a);
        SDL_FRect rect = {sx, sy, tilePx, tilePx};
        SDL_RenderFillRectF(renderer_, &rect);
        
        // Door state indicator for color fallback
        if (elev.state == ElevatorState::Boarding || elev.state == ElevatorState::DoorOpening) {
            SDL_SetRenderDrawColor(renderer_, 255, 255, 200, 180);
            SDL_RenderDrawRectF(renderer_, &rect);
        }
    }
}

SDL_Texture* ElevatorRenderer::loadTexture(const std::string& path)
{
    SDL_Texture* texture = IMG_LoadTexture(renderer_, path.c_str());
    if (!texture) {
        spdlog::warn("ElevatorRenderer: Failed to load texture {}: {}", path, IMG_GetError());
        return nullptr;
    }
    return texture;
}

void ElevatorRenderer::freeTextures()
{
    if (texCar_) {
        SDL_DestroyTexture(texCar_);
        texCar_ = nullptr;
    }
    if (texOpen_) {
        SDL_DestroyTexture(texOpen_);
        texOpen_ = nullptr;
    }
}

} // namespace vse