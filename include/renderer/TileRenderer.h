#pragma once
#include "core/IRenderCommand.h"
#include <SDL.h>
#include <SDL_image.h>
#include <string>

namespace vse {

class TileRenderer {
public:
    explicit TileRenderer(SDL_Renderer* renderer);
    ~TileRenderer();

    // Load all tile sprites from VSE_PROJECT_ROOT/content/sprites/
    // Returns true if at least one sprite loaded; missing sprites use color fallback
    bool loadSprites(const std::string& spritesDir);

    // Draw a tile — uses sprite if loaded, else color fallback
    void drawTile(const RenderTile& tile, float sx, float sy, float tilePx);

    // 명시적 텍스처 해제 — SDL_DestroyRenderer 전에 호출 필수
    void freeTextures();

    // 복사/이동 금지 — SDL_Texture* 소유권 때문에
    TileRenderer(const TileRenderer&) = delete;
    TileRenderer& operator=(const TileRenderer&) = delete;
    TileRenderer(TileRenderer&&) = delete;
    TileRenderer& operator=(TileRenderer&&) = delete;

private:
    SDL_Renderer* renderer_;
    SDL_Texture* texOffice_      = nullptr;
    SDL_Texture* texResidential_ = nullptr;
    SDL_Texture* texCommercial_  = nullptr;
    SDL_Texture* texLobby_       = nullptr;
    SDL_Texture* texShaft_       = nullptr;
    SDL_Texture* texFacade_      = nullptr;

    SDL_Texture* loadTexture(const std::string& path);
};

} // namespace vse