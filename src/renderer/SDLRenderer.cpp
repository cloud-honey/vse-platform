#include "renderer/SDLRenderer.h"
#include <SDL.h>
#include <spdlog/spdlog.h>
#include <cmath>

namespace vse {

SDLRenderer::SDLRenderer() = default;

SDLRenderer::~SDLRenderer()
{
    shutdown();
}

bool SDLRenderer::init(int windowW, int windowH, const char* title)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowW, windowH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window_) {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(
        window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer_) {
        spdlog::error("SDL_CreateRenderer failed: {}", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    spdlog::info("SDLRenderer::init OK ({}x{})", windowW, windowH);
    return true;
}

void SDLRenderer::shutdown()
{
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow(window_);     window_   = nullptr; }
}

void SDLRenderer::render(const RenderFrame& frame, const Camera& camera)
{
    if (!renderer_) return;

    // 배경 클리어 — 하늘색
    SDL_SetRenderDrawColor(renderer_, 135, 206, 235, 255);
    SDL_RenderClear(renderer_);

    // 건물 배경 (건설된 층)
    drawGridBackground(frame, camera);

    // 타일 렌더링 (테넌트 등)
    drawTiles(frame, camera);

    // 엘리베이터 렌더링
    drawElevators(frame, camera);

    // 그리드 선
    if (frame.drawGrid) {
        drawGridLines(frame, camera);
    }

    // 층 번호 라벨
    drawFloorLabels(frame, camera);

    SDL_RenderPresent(renderer_);
}

void SDLRenderer::drawGridBackground(const RenderFrame& frame, const Camera& camera)
{
    // 건설된 층 영역을 어두운 배경으로
    // RenderFrame의 tiles에 포함된 층 범위를 기반으로 배경 그리기
    // Phase 1: floor 0은 항상 존재 → 전체 maxFloors 영역에 옅은 배경

    int ts = frame.tileSize;
    for (int f = 0; f < frame.maxFloors; ++f) {
        float sx = camera.tileToScreenX(0);
        float sy = camera.tileToScreenY(f);
        float w  = frame.floorWidth * ts * camera.zoomLevel();
        float h  = ts * camera.zoomLevel();

        // 화면 밖이면 스킵
        if (sy + h < 0 || sy > camera.viewportH()) continue;
        if (sx + w < 0 || sx > camera.viewportW()) continue;

        // 층 0(로비)만 살짝 다른 색
        if (f == 0) {
            SDL_SetRenderDrawColor(renderer_, 60, 60, 80, 200);
        } else {
            SDL_SetRenderDrawColor(renderer_, 40, 42, 54, 180);
        }
        SDL_FRect rect = {sx, sy, w, h};
        SDL_RenderFillRectF(renderer_, &rect);
    }
}

void SDLRenderer::drawGridLines(const RenderFrame& frame, const Camera& camera)
{
    int ts = frame.tileSize;
    float zoom = camera.zoomLevel();

    // 줌이 작으면 그리드 선 생략 (0.5 이하)
    if (zoom < 0.5f) return;

    SDL_SetRenderDrawColor(renderer_, 80, 80, 100, 60);

    // 수평선 (층 경계)
    for (int f = 0; f <= frame.maxFloors; ++f) {
        float y = camera.tileToScreenY(f);
        if (y < -1 || y > camera.viewportH() + 1) continue;
        float x1 = camera.tileToScreenX(0);
        float x2 = camera.tileToScreenX(frame.floorWidth);
        SDL_RenderDrawLineF(renderer_, x1, y, x2, y);
    }

    // 수직선 (타일 경계)
    for (int x = 0; x <= frame.floorWidth; ++x) {
        float sx = camera.tileToScreenX(x);
        if (sx < -1 || sx > camera.viewportW() + 1) continue;
        float y1 = camera.tileToScreenY(0);
        float y2 = camera.tileToScreenY(frame.maxFloors - 1);
        SDL_RenderDrawLineF(renderer_, sx, y2, sx, y1);
    }
}

void SDLRenderer::drawTiles(const RenderFrame& frame, const Camera& camera)
{
    float zoom = camera.zoomLevel();
    int ts = frame.tileSize;
    float tilePx = ts * zoom;

    for (const auto& tile : frame.tiles) {
        float sx = camera.tileToScreenX(tile.x);
        float sy = camera.tileToScreenY(tile.floor);

        // 화면 밖 컬링
        if (sx + tilePx < 0 || sx > camera.viewportW()) continue;
        if (sy + tilePx < 0 || sy > camera.viewportH()) continue;

        SDL_SetRenderDrawColor(renderer_, tile.color.r, tile.color.g,
                                tile.color.b, tile.color.a);
        SDL_FRect rect = {sx, sy, tilePx, tilePx};
        SDL_RenderFillRectF(renderer_, &rect);
    }
}

void SDLRenderer::drawElevators(const RenderFrame& frame, const Camera& camera)
{
    float zoom = camera.zoomLevel();
    int ts = frame.tileSize;
    float tilePx = ts * zoom;

    for (const auto& elev : frame.elevators) {
        // 보간 위치 사용 (Phase 1: currentFloor와 동일)
        float worldY = (elev.interpolatedFloor + 1) * ts;
        float sx = camera.worldToScreenX(static_cast<float>(elev.shaftX * ts));
        float sy = camera.worldToScreenY(worldY);

        SDL_SetRenderDrawColor(renderer_, elev.color.r, elev.color.g,
                                elev.color.b, elev.color.a);
        SDL_FRect rect = {sx, sy, tilePx, tilePx};
        SDL_RenderFillRectF(renderer_, &rect);

        // 문 상태 표시 (Boarding/DoorOpening: 밝은 테두리)
        if (elev.state == ElevatorState::Boarding ||
            elev.state == ElevatorState::DoorOpening) {
            SDL_SetRenderDrawColor(renderer_, 255, 255, 200, 180);
            SDL_RenderDrawRectF(renderer_, &rect);
        }
    }
}

void SDLRenderer::drawFloorLabels(const RenderFrame& frame, const Camera& camera)
{
    // Phase 1: SDL2_ttf 없이는 텍스트 못 그림 → 로비(0층)만 색으로 표시
    // 실제 텍스트는 Dear ImGui 오버레이로 처리 예정
    (void)frame;
    (void)camera;
}

} // namespace vse
