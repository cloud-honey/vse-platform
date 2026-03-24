#pragma once
#include "core/IRenderCommand.h"
#include "renderer/Camera.h"

// Forward declarations — SDL2 타입 (include 금지 in headers)
struct SDL_Window;
struct SDL_Renderer;

namespace vse {

/**
 * SDLRenderer — Layer 3 렌더러. RenderFrame을 SDL2로 그린다.
 *
 * 책임:
 * - SDL2 윈도우/렌더러 생성·파괴
 * - RenderFrame 소비 → 화면 출력
 * - 타일 그리드, 엘리베이터, 디버그 텍스트 그리기
 * - Domain 시스템 참조 금지 (RenderFrame만 받는다)
 */
class SDLRenderer {
public:
    SDLRenderer();
    ~SDLRenderer();

    // 초기화/정리
    bool init(int windowW, int windowH, const char* title);
    void shutdown();

    // 프레임 렌더링
    void render(const RenderFrame& frame, const Camera& camera);

    // SDL 핸들 (Dear ImGui 초기화용)
    SDL_Window*   window()   const { return window_; }
    SDL_Renderer* renderer() const { return renderer_; }

    bool isValid() const { return window_ != nullptr && renderer_ != nullptr; }

private:
    // 그리드 렌더링
    void drawGridBackground(const RenderFrame& frame, const Camera& camera);
    void drawGridLines(const RenderFrame& frame, const Camera& camera);
    void drawTiles(const RenderFrame& frame, const Camera& camera);
    void drawElevators(const RenderFrame& frame, const Camera& camera);
    void drawFloorLabels(const RenderFrame& frame, const Camera& camera);

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

} // namespace vse
