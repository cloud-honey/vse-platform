#include "renderer/TileRenderer.h"
#include <spdlog/spdlog.h>

namespace vse {

TileRenderer::TileRenderer(SDL_Renderer* renderer)
    : renderer_(renderer)
{
}

TileRenderer::~TileRenderer()
{
    freeTextures();
}

bool TileRenderer::loadSprites(const std::string& spritesDir)
{
    freeTextures();  // 재호출 시 기존 텍스처 누수 방지
    bool anyLoaded = false;
    
    texOffice_ = loadTexture(spritesDir + "/office_floor.png");
    if (texOffice_) anyLoaded = true;
    
    texResidential_ = loadTexture(spritesDir + "/residential_floor.png");
    if (texResidential_) anyLoaded = true;
    
    texCommercial_ = loadTexture(spritesDir + "/commercial_floor.png");
    if (texCommercial_) anyLoaded = true;
    
    texLobby_ = loadTexture(spritesDir + "/lobby_floor.png");
    if (texLobby_) anyLoaded = true;
    
    texShaft_ = loadTexture(spritesDir + "/elevator_shaft.png");
    if (texShaft_) anyLoaded = true;
    
    texFacade_ = loadTexture(spritesDir + "/building_facade.png");
    if (texFacade_) anyLoaded = true;
    
    if (!anyLoaded) {
        spdlog::warn("TileRenderer: No tile sprites loaded, will use color fallback");
    } else {
        spdlog::info("TileRenderer: Loaded tile sprites from {}", spritesDir);
    }
    
    return anyLoaded;
}

void TileRenderer::drawTile(const RenderTile& tile, float sx, float sy, float tilePx)
{
    SDL_Texture* texture = nullptr;
    
    // Select texture based on tile properties
    if (tile.isElevatorShaft) {
        // 엘리베이터 샤프트 — 텍스처 사용 (isLobby보다 우선)
        texture = texShaft_;
    } else if (tile.isFacade) {
        // 건물 외벽/빈 타일 — nullptr이면 컬러 폴백 (isElevatorShaft와 동일 패턴)
        texture = texFacade_;
    } else if (tile.isLobby && texLobby_) {
        texture = texLobby_;
    } else {
        switch (tile.tenantType) {
            case TenantType::Office:
                texture = texOffice_;
                break;
            case TenantType::Residential:
                texture = texResidential_;
                break;
            case TenantType::Commercial:
                texture = texCommercial_;
                break;
            case TenantType::None:
            default:
                texture = nullptr;
                break;
        }
    }
    
    if (texture) {
        // Draw sprite
        SDL_FRect dest = {sx, sy, tilePx, tilePx};
        SDL_RenderCopyF(renderer_, texture, nullptr, &dest);
    } else {
        // Color fallback
        SDL_SetRenderDrawColor(renderer_, tile.color.r, tile.color.g,
                                tile.color.b, tile.color.a);
        SDL_FRect rect = {sx, sy, tilePx, tilePx};
        SDL_RenderFillRectF(renderer_, &rect);
    }
}

SDL_Texture* TileRenderer::loadTexture(const std::string& path)
{
    SDL_Texture* texture = IMG_LoadTexture(renderer_, path.c_str());
    if (!texture) {
        spdlog::warn("TileRenderer: Failed to load texture {}: {}", path, IMG_GetError());
        return nullptr;
    }
    return texture;
}

void TileRenderer::freeTextures()
{
    if (texOffice_) {
        SDL_DestroyTexture(texOffice_);
        texOffice_ = nullptr;
    }
    if (texResidential_) {
        SDL_DestroyTexture(texResidential_);
        texResidential_ = nullptr;
    }
    if (texCommercial_) {
        SDL_DestroyTexture(texCommercial_);
        texCommercial_ = nullptr;
    }
    if (texLobby_) {
        SDL_DestroyTexture(texLobby_);
        texLobby_ = nullptr;
    }
    if (texShaft_) {
        SDL_DestroyTexture(texShaft_);
        texShaft_ = nullptr;
    }
    if (texFacade_) {
        SDL_DestroyTexture(texFacade_);
        texFacade_ = nullptr;
    }
}

} // namespace vse