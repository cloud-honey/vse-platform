#pragma once
#include "core/IRenderCommand.h"
#include "renderer/Camera.h"
#include "renderer/DebugPanel.h"
#include "renderer/HUDPanel.h"
#include "renderer/SaveLoadPanel.h"
#include "renderer/SpriteSheet.h"
#include "renderer/AnimationSystem.h"
#include "renderer/FontManager.h"
#include "renderer/BuildCursor.h"
#include "renderer/AudioEngine.h"
#include "renderer/DayNightCycle.h"
#include "renderer/TileRenderer.h"
#include "renderer/ElevatorRenderer.h"
#include <unordered_map>
#include <cstdint>
#include <memory>

// Forward declarations — SDL2 타입 (include 금지 in headers)
struct SDL_Window;
struct SDL_Renderer;

namespace vse {
class EventBus;  // forward declaration

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
    bool init(int windowW, int windowH, const char* title, EventBus& bus);
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
    
    // SaveLoadPanel 접근
    SaveLoadPanel& saveLoadPanel() { return saveLoadPanel_; }
    
    // Save/Load slot handling
    bool checkPendingSave(int& outSlotIndex);
    bool checkPendingLoad(int& outSlotIndex);
    
    // Check if SaveLoadPanel is currently open
    bool isSaveLoadPanelOpen() const { return saveLoadPanel_.isOpen(); }
    
    // Tenant selection handling (TASK-05-001)
    bool checkTenantSelection(int& outTenantType);
    void setShouldOpenTenantPopup(bool open);
    
    // HUD interaction handling (TASK-05-004)
    bool checkPendingBuildAction(int& outBuildAction, int& outTenantType);
    bool checkPendingSpeedChange(int& outSpeedMultiplier);
    // 메뉴 버튼 액션: 1=NewGame, 2=LoadGame, 3=Quit, 4=Resume, 5=Save, 6=MainMenu
    bool checkPendingMenuAction(int& outAction);
    
    // AudioEngine 접근
    AudioEngine& audioEngine() { return audioEngine_; }

private:
    // 그리드 렌더링
    void drawGridBackground(const RenderFrame& frame, const Camera& camera);
    void drawGridLines(const RenderFrame& frame, const Camera& camera);
    void drawTiles(const RenderFrame& frame, const Camera& camera);
    void drawElevators(const RenderFrame& frame, const Camera& camera);
    void drawAgents(const RenderFrame& frame, const Camera& camera, float dt);
    void drawFloorLabels(const RenderFrame& frame, const Camera& camera);
    void drawGameStateUI(const RenderFrame& frame);

    SDL_Window*   window_      = nullptr;
    SDL_Renderer* renderer_    = nullptr;
    DebugPanel    debugPanel_;              // Design Spec: 별도 구성요소로 분리
    HUDPanel      hudPanel_;                // 게임 HUD 패널
    SaveLoadPanel saveLoadPanel_;           // Save/Load UI 패널
    BuildCursor   buildCursor_;             // 건설 모드 커서 피드백
    AudioEngine   audioEngine_;             // 오디오 엔진
    std::unique_ptr<DayNightCycle> dayNightCycle_;  // 주야간 사이클
    EventBus*     bus_         = nullptr;   // 이벤트 버스 참조 (DayNightCycle용)
    
    // Sprite sheet system
    std::unique_ptr<SpriteSheet> npcSheet_;
    AnimationSystem animationSystem_;
    
    // Tile rendering system
    std::unique_ptr<TileRenderer> tileRenderer_;
    
    // Elevator rendering system
    std::unique_ptr<ElevatorRenderer> elevatorRenderer_;
    
    // Font system
    FontManager fontManager_;
    // Floor label texture cache: key = floor index, value = {texture, w, h}
    struct LabelTexture { SDL_Texture* tex = nullptr; int w = 0; int h = 0; };
    std::unordered_map<int, LabelTexture> labelCache_;
    void clearLabelCache();

    // Frame time tracking for animation
    float lastFrameTime_ = 0.0f;
    
    // HiDPI scaling factors
    float dpiScaleX_ = 1.0f;
    float dpiScaleY_ = 1.0f;
    
    // Tenant selection state (TASK-05-001)
    bool shouldOpenTenantPopup_ = false;
    int pendingTenantSelection_ = -1; // -1 means no selection pending
    
    // Save/Load pending state
    int pendingSaveSlot_ = -1;  // -1 means no save pending
    int pendingLoadSlot_ = -1;  // -1 means no load pending
    
    // HUD interaction pending state (TASK-05-004)
    int pendingBuildAction_ = 0;      // 0=none, 1=floor, 2=office, 3=residential, 4=commercial
    int pendingMenuAction_  = 0;      // 0=none, 1=NewGame, 2=LoadGame, 3=Quit, 4=Resume, 5=Save, 6=MainMenu
    int pendingSpeedChange_ = -1;     // -1 means no speed change pending
};

} // namespace vse
