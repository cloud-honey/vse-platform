#pragma once
/**
 * @file Bootstrapper.h
 * @layer Core Runtime (src/core/)
 * @task TASK-02-001
 * @author 붐 (Claude Sonnet 4.6)
 * @reviewed GPT-5.4 Thinking, Gemini 3 Flash, Claude (pass, 2026-03-25)
 *
 * @brief Composition Root — 모든 시스템 소유 및 초기화 순서 관리.
 *        main()은 init() → run() → shutdown() 3줄만 호출.
 *
 * @note 시간 진행의 단일 소유자: simClock_
 *       배속/pause 상태는 simClock_에 위임.
 *       accumulator 기반 배속 루프 횟수 계산은 Bootstrapper 담당.
 *
 * @see CLAUDE.md §메인 루프 규칙
 * @see VSE_Design_Spec.md §Bootstrapper
 */

#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include "core/SimClock.h"
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

/**
 * Bootstrapper — Composition Root.
 *
 * 역할:
 *  - 모든 시스템 소유 및 초기화 순서 관리
 *  - 메인 루프 캡슐화 (handleInput → updateSim → render)
 *  - main()은 init() → run() → shutdown() 3줄만 호출
 *
 * 시간 진행의 단일 소유자: simClock_
 *  - pause/resume/setSpeed: simClock_ 위임
 *  - accumulator 및 배속 루프 횟수 계산: Bootstrapper 담당
 *  - currentTick, isPaused, speed 조회: simClock_ 위임
 *
 * @layer Core Runtime (src/core/)
 * @see CLAUDE.md §메인 루프 규칙
 */
class Bootstrapper {
public:
    Bootstrapper() = default;

    // 모든 시스템 초기화 (SDL 포함)
    // @return false 시 main()이 즉시 종료
    bool init();

    // 메인 루프 실행 (SDL 윈도우 폐쇄 또는 Quit 커맨드까지)
    void run();

    // 정리 (SDL 종료)
    void shutdown();

    // SDL 없이 Domain만 조립 (headless 단위 테스트용)
    // @param configPath assets/로 시작하면 VSE_PROJECT_ROOT 기준 절대경로로 변환
    bool initDomainOnly(const std::string& configPath = "assets/config/game_config.json");

#ifdef VSE_TESTING
    // ── 테스트 전용 접근자 (VSE_TESTING 빌드에서만 노출) ──
    // 프로덕션 빌드에서는 심볼 생성 안 됨
    bool testGetPaused() const  { return simClock_.isPaused(); }
    int  testGetSpeed()  const  { return simClock_.speed(); }
    bool testGetRunning() const { return running_; }
    void testProcessCommands(const std::vector<GameCommand>& cmds) {
        processCommands(cmds, running_);
    }
#endif

private:
    // ── 커맨드 처리 (frame 기준 1회 호출) ──────────────
    // 카메라/시스템 커맨드는 즉시 반영, 도메인 커맨드는 그 즉시 Domain 호출.
    // NOTE: tick 루프 밖에서 호출되므로 "첫 tick에서만 drain"은
    //       frame당 1회 보장으로 동일하게 충족됨.
    void processCommands(const std::vector<GameCommand>& cmds, bool& running);

    // ── DebugInfo 채우기 (render 직전 호출) ────────────
    void fillDebugInfo(RenderFrame& frame, int realDeltaMs);

    // ── 초기 씬 설정 헬퍼 (init/initDomainOnly 공용) ───
    // 프로토타입 씬: 5층 건물 + 엘리베이터 1축 2대 + NPC 5명
    void setupInitialScene();

    // ── 실행 상태 ───────────────────────────────────────
    bool running_     = false;
    int  accumulator_ = 0;

    // ── Core Runtime ────────────────────────────────────
    // 선언 순서 = 초기화 순서 (simClock_은 eventBus_ 이후)
    ConfigManager config_;
    EventBus      eventBus_;
    SimClock      simClock_{eventBus_};  // 시간 진행의 단일 소유자
    entt::registry registry_;

    // ── Domain (Layer 1) ────────────────────────────────
    std::unique_ptr<GridSystem>     grid_;
    std::unique_ptr<AgentSystem>    agents_;
    std::unique_ptr<TransportSystem> transport_;

    // ── Renderer (Layer 3) ──────────────────────────────
    SDLRenderer  sdlRenderer_;
    Camera       camera_;
    InputMapper  inputMapper_;
    std::unique_ptr<RenderFrameCollector> collector_;

    // ── 설정 캐시 (init() 시 ConfigManager에서 읽음) ───
    int   windowW_   = 0;
    int   windowH_   = 0;
    int   tileSizePx_ = 0;
    int   tickMs_    = 100;
    float zoomMin_   = 0.25f;
    float zoomMax_   = 4.0f;
    float panSpeed_  = 8.0f;

    // ── 렌더링 상태 ─────────────────────────────────────
    bool drawGrid_  = true;
    bool drawDebug_ = true;
};

} // namespace vse
