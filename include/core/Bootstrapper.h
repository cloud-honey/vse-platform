#pragma once

#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include "domain/GridSystem.h"
#include "domain/AgentSystem.h"
#include "domain/TransportSystem.h"
#include "renderer/SDLRenderer.h"
#include "renderer/Camera.h"
#include "renderer/InputMapper.h"
#include "renderer/RenderFrameCollector.h"
#include "core/InputTypes.h"
#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace vse {

class Bootstrapper {
public:
    Bootstrapper() = default;
    
    // 모든 시스템 초기화 (SDL 포함)
    bool init();
    
    // 메인 루프 실행
    void run();
    
    // 정리
    void shutdown();
    
    // SDL 없이 Domain만 조립 (테스트용)
    bool initDomainOnly(const std::string& configPath = "assets/config/game_config.json");

    // 테스트 전용 접근자 (headless 단위 테스트용)
    bool testGetPaused() const  { return paused_; }
    int  testGetSpeed() const   { return speed_; }
    bool testGetRunning() const { return running_; }
    void testProcessCommands(const std::vector<GameCommand>& cmds) {
        processCommands(cmds, running_);
    }

private:
    // 커맨드 처리
    void processCommands(const std::vector<GameCommand>& cmds, bool& running);
    
    // DebugInfo 채우기
    void fillDebugInfo(RenderFrame& frame, int realDeltaMs);
    
    // 게임 상태
    int speed_ = 1;
    bool paused_ = false;
    bool running_ = false;
    SimTick currentTick_ = 0;
    int accumulator_ = 0;
    
    // 시스템들
    ConfigManager config_;
    EventBus eventBus_;
    entt::registry registry_;
    std::unique_ptr<GridSystem> grid_;
    std::unique_ptr<AgentSystem> agents_;
    std::unique_ptr<TransportSystem> transport_;
    SDLRenderer sdlRenderer_;
    Camera camera_;
    InputMapper inputMapper_;
    std::unique_ptr<RenderFrameCollector> collector_;
    
    // 설정값 캐시
    int windowW_ = 0;
    int windowH_ = 0;
    int tileSizePx_ = 0;
    int tickMs_ = 0;
    float zoomMin_ = 0.0f;
    float zoomMax_ = 0.0f;
    float panSpeed_ = 0.0f;
    
    // 렌더링 상태
    bool drawGrid_ = true;
    bool drawDebug_ = true;
    
};

} // namespace vse