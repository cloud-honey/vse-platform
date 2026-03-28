#pragma once
#include "core/IRenderCommand.h"
#include <SDL.h>
#include <SDL_image.h>
#include <string>

namespace vse {

class ElevatorRenderer {
public:
    explicit ElevatorRenderer(SDL_Renderer* renderer);
    ~ElevatorRenderer();

    bool loadSprites(const std::string& spritesDir);

    // Draw elevator car — uses sprite if loaded, else color fallback
    void drawElevator(const RenderElevator& elev, float sx, float sy, float tilePx);

    // 명시적 텍스처 해제 — SDL_DestroyRenderer 전에 호출 필수
    void freeTextures();

    // 복사/이동 금지 — SDL_Texture* 소유권 때문에
    ElevatorRenderer(const ElevatorRenderer&) = delete;
    ElevatorRenderer& operator=(const ElevatorRenderer&) = delete;
    ElevatorRenderer(ElevatorRenderer&&) = delete;
    ElevatorRenderer& operator=(ElevatorRenderer&&) = delete;

private:
    SDL_Renderer* renderer_;
    SDL_Texture* texCar_  = nullptr;  // elevator_car.png (doors closed)
    SDL_Texture* texOpen_ = nullptr;  // elevator_open.png (doors open)

    SDL_Texture* loadTexture(const std::string& path);
};

} // namespace vse