#pragma once
#include "core/IRenderCommand.h"
#include "renderer/Camera.h"
#include "renderer/DebugPanel.h"
#include "renderer/HUDPanel.h"
#include "renderer/SpriteSheet.h"
#include "renderer/AnimationSystem.h"
#include "renderer/FontManager.h"
#include <unordered_map>
#include <cstdint>

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
 * - 타일 그리드, 엘리베이터, NPC 그리기
 * - Domain 시스템 참조 금지 (RenderFrame만 받는다)
 *
 * DebugPanel은 SDLRenderer가 소유하는 별도 구성요소 (Design Spec §디렉터리).
 * debugPanel() 접근자로 토글 제어 가능.
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

    // DebugPanel 접근 (F3 토글 등 외부 제어용)
    DebugPanel& debugPanel() { return debugPanel_; }
    
    // HUDPanel 접근
    HUDPanel& hudPanel() { return hudPanel_; }

private:
    // 그리드 렌더링
    void drawGridBackground(const RenderFrame& frame, const Camera& camera);
    void drawGridLines(const RenderFrame& frame, const Camera& camera);
    void drawTiles(const RenderFrame& frame, const Camera& camera);
    void drawElevators(const RenderFrame& frame, const Camera& camera);
    void drawAgents(const RenderFrame& frame, const Camera& camera, float dt);
    void drawFloorLabels(const RenderFrame& frame, const Camera& camera);

    SDL_Window*   window_      = nullptr;
    SDL_Renderer* renderer_    = nullptr;
    DebugPanel    debugPanel_;              // Design Spec: 별도 구성요소로 분리
    HUDPanel      hudPanel_;                // 게임 HUD 패널
    
    // Sprite sheet system
    std::unique_ptr<SpriteSheet> npcSheet_;
    AnimationSystem animationSystem_;
    
    // Font system
    FontManager fontManager_;
    // Floor label texture cache: key = floor index, value = {texture, w, h}
    struct LabelTexture { SDL_Texture* tex = nullptr; int w = 0; int h = 0; };
    std::unordered_map<int, LabelTexture> labelCache_;
    void clearLabelCache();

    // Frame time tracking for animation
    float lastFrameTime_ = 0.0f;
};

} // namespace vse
