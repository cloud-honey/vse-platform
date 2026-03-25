/**
 * @file Bootstrapper.cpp
 * @layer Core Runtime (src/core/)
 * @task TASK-02-001
 * @author 붐 (Claude Sonnet 4.6)
 * @reviewed GPT-5.4 Thinking, Gemini 3 Flash, Claude (pass, 2026-03-25)
 *
 * @brief Bootstrapper 구현.
 *        init() — 시스템 조립 + SDL 초기화
 *        run()  — fixed-tick 메인 루프 (CLAUDE.md §메인 루프 규칙 준수)
 *        shutdown() — 정리
 *        initDomainOnly() — headless 테스트용 Domain 전용 초기화
 */
#include "core/Bootstrapper.h"
#include <spdlog/spdlog.h>
#include <SDL.h>

namespace vse {

// MAX_TICKS_PER_FRAME: spiral-of-death 방지 상한.
// 4x 배속 기준 상한 8 (speed * 2).
// 이 이상은 프레임 드랍 허용하고 accumulator 리셋.
static constexpr int MAX_TICKS_PER_FRAME = 8;

// ──────────────────────────────────────────────────────────
// init — 전체 시스템 초기화 (SDL 포함)
// ──────────────────────────────────────────────────────────

bool Bootstrapper::init() {
    spdlog::info("Tower Tycoon starting...");

    // ── Config 로딩 ─────────────────────────────────────
    config_.loadFromFile(std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json");
    if (!config_.isLoaded()) {
        spdlog::error("Failed to load config");
        return false;
    }

    windowW_    = config_.getInt("rendering.windowWidth",  1280);
    windowH_    = config_.getInt("rendering.windowHeight", 720);
    tileSizePx_ = config_.getInt("grid.tileSizePx",        32);
    tickMs_     = config_.getInt("simulation.tickRateMs",  100);
    zoomMin_    = config_.getFloat("rendering.zoomMin",    0.25f);
    zoomMax_    = config_.getFloat("rendering.zoomMax",    4.0f);
    panSpeed_   = config_.getFloat("rendering.cameraPanSpeed", 8.0f);

    // ── SDL2 + Renderer 초기화 ──────────────────────────
    if (!sdlRenderer_.init(windowW_, windowH_, "Tower Tycoon")) {
        return false;
    }

    // ── Domain 시스템 조립 ──────────────────────────────
    grid_      = std::make_unique<GridSystem>(eventBus_, config_);
    agents_    = std::make_unique<AgentSystem>(*grid_, eventBus_);
    transport_ = std::make_unique<TransportSystem>(*grid_, eventBus_, config_);

    setupInitialScene();

    // ── Layer 3 초기화 (Config 기반) ────────────────────
    camera_ = Camera(windowW_, windowH_, tileSizePx_, zoomMin_, zoomMax_);
    camera_.centerOn(
        static_cast<float>(grid_->floorWidth() * tileSizePx_ / 2),
        static_cast<float>(2 * tileSizePx_)
    );
    inputMapper_.setPanSpeed(panSpeed_);
    collector_ = std::make_unique<RenderFrameCollector>(*grid_, *transport_, tileSizePx_);
    collector_->setAgentSource(agents_.get(), &registry_);

    running_   = true;
    drawGrid_  = true;
    drawDebug_ = true;

    spdlog::info("Bootstrapper initialized successfully");
    return true;
}

// ──────────────────────────────────────────────────────────
// initDomainOnly — SDL 없이 Domain만 조립 (테스트용)
// ──────────────────────────────────────────────────────────

bool Bootstrapper::initDomainOnly(const std::string& configPath) {
    std::string resolvedPath = configPath;
    if (configPath.rfind("assets/", 0) == 0) {
        resolvedPath = std::string(VSE_PROJECT_ROOT) + "/" + configPath;
    }
    config_.loadFromFile(resolvedPath);
    if (!config_.isLoaded()) {
        spdlog::error("Failed to load config from {}", configPath);
        return false;
    }

    tileSizePx_ = config_.getInt("grid.tileSizePx",       32);
    tickMs_     = config_.getInt("simulation.tickRateMs", 100);

    grid_      = std::make_unique<GridSystem>(eventBus_, config_);
    agents_    = std::make_unique<AgentSystem>(*grid_, eventBus_);
    transport_ = std::make_unique<TransportSystem>(*grid_, eventBus_, config_);

    setupInitialScene();

    running_ = true;
    spdlog::info("Domain-only initialization completed");
    return true;
}

// ──────────────────────────────────────────────────────────
// setupInitialScene — 프로토타입 씬 공통 설정
// ──────────────────────────────────────────────────────────

void Bootstrapper::setupInitialScene() {
    // 5층 건물 + 엘리베이터 1축 2대
    for (int f = 1; f <= 4; ++f) grid_->buildFloor(f);
    grid_->placeElevatorShaft(5, 0, 4);
    transport_->createElevator(5, 0, 4, 8);
    transport_->createElevator(5, 0, 2, 4);

    // 테넌트 배치
    EntityId homeId1 = registry_.create();
    EntityId homeId2 = registry_.create();
    EntityId homeId3 = registry_.create();
    EntityId workId1 = registry_.create();
    EntityId workId2 = registry_.create();

    grid_->placeTenant({0, 0}, TenantType::Residential, 2, homeId1);
    grid_->placeTenant({3, 0}, TenantType::Residential, 2, homeId2);
    grid_->placeTenant({6, 0}, TenantType::Residential, 2, homeId3);
    grid_->placeTenant({0, 2}, TenantType::Office,      3, workId1);
    grid_->placeTenant({8, 2}, TenantType::Office,      3, workId2);

    // NPC 스폰
    agents_->spawnAgent(registry_, homeId1, workId1);
    agents_->spawnAgent(registry_, homeId2, workId1);
    agents_->spawnAgent(registry_, homeId3, workId2);
    agents_->spawnAgent(registry_, homeId1, workId2);
    agents_->spawnAgent(registry_, homeId2, workId2);
}

// ──────────────────────────────────────────────────────────
// run — 메인 루프
// ──────────────────────────────────────────────────────────

void Bootstrapper::run() {
    Uint32 lastFrameMs = SDL_GetTicks();

    while (running_) {
        Uint32 nowMs      = SDL_GetTicks();
        int realDeltaMs   = static_cast<int>(nowMs - lastFrameMs);
        lastFrameMs       = nowMs;

        // ── 1. handleInput ──────────────────────────────
        std::vector<GameCommand> commands;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            inputMapper_.processEvent(event, commands);
        }
        inputMapper_.processHeldKeys(commands);

        // ── 커맨드 처리 (frame 기준 1회) ────────────────
        // 입력 수집(frame) → 즉시 처리 → tick loop 진입.
        // 도메인 상태 변경(pause/speed)은 이 호출 이후 tick loop에 반영됨.
        processCommands(commands, running_);

        // ── 2. updateSim ────────────────────────────────
        // simClock_이 시간 진행의 단일 소유자.
        // Bootstrapper는 accumulator로 배속 루프 횟수를 제어,
        // simClock_.advanceTick()으로 실제 tick을 위임.
        if (!simClock_.isPaused()) {
            accumulator_ += realDeltaMs * simClock_.speed();
            int ticksThisFrame = 0;

            while (accumulator_ >= tickMs_ && ticksThisFrame < MAX_TICKS_PER_FRAME) {
                accumulator_ -= tickMs_;
                ticksThisFrame++;

                eventBus_.flush();                            // tick N-1 이벤트 배달
                simClock_.advanceTick();                      // tick N 진행 + TickAdvanced 발행
                GameTime time = simClock_.currentGameTime();

                agents_->update(registry_, time);
                transport_->update(time);
            }

            // spiral-of-death 방지: 누적 과다 시 리셋 (디버거 중단 재개 등 대비)
            if (accumulator_ > tickMs_ * MAX_TICKS_PER_FRAME) {
                accumulator_ = 0;
            }
        }

        // ── 3. render ───────────────────────────────────
        collector_->setDrawGrid(drawGrid_);
        collector_->setDrawDebugInfo(drawDebug_);
        RenderFrame frame = collector_->collect();
        fillDebugInfo(frame, realDeltaMs);
        sdlRenderer_.render(frame, camera_);
    }
}

// ──────────────────────────────────────────────────────────
// processCommands — 커맨드 처리
// ──────────────────────────────────────────────────────────

void Bootstrapper::processCommands(const std::vector<GameCommand>& cmds, bool& running) {
    for (const auto& cmd : cmds) {
        switch (cmd.type) {
        case CommandType::Quit:
            running  = false;
            running_ = false;
            break;
        case CommandType::TogglePause:
            if (simClock_.isPaused()) simClock_.resume();
            else                      simClock_.pause();
            spdlog::info("Pause: {}", simClock_.isPaused() ? "ON" : "OFF");
            break;
        case CommandType::SetSpeed:
            simClock_.setSpeed(cmd.setSpeed.speedMultiplier);
            spdlog::info("Speed: {}x", simClock_.speed());
            break;
        case CommandType::CameraPan:
            camera_.pan(cmd.cameraPan.deltaX, cmd.cameraPan.deltaY);
            break;
        case CommandType::CameraZoom:
            camera_.zoom(cmd.cameraZoom.zoomDelta);
            break;
        case CommandType::CameraReset:
            camera_.reset();
            break;
        case CommandType::ToggleDebugOverlay:
            drawDebug_ = !drawDebug_;
            drawGrid_  = drawDebug_;
            sdlRenderer_.debugPanel().setVisible(drawDebug_);
            break;

        // ── 도메인 커맨드 ──────────────────────────────
        case CommandType::BuildFloor:
            grid_->buildFloor(cmd.buildFloor.floor);
            break;
        case CommandType::PlaceTenant:
            grid_->placeTenant(
                TileCoord{cmd.placeTenant.x, cmd.placeTenant.floor},
                static_cast<TenantType>(cmd.placeTenant.tenantType),
                cmd.placeTenant.width,
                INVALID_ENTITY);
            break;
        case CommandType::PlaceElevatorShaft:
            grid_->placeElevatorShaft(
                cmd.placeShaft.shaftX,
                cmd.placeShaft.bottomFloor,
                cmd.placeShaft.topFloor);
            break;
        case CommandType::CreateElevator:
            transport_->createElevator(
                cmd.createElevator.shaftX,
                cmd.createElevator.bottomFloor,
                cmd.createElevator.topFloor,
                cmd.createElevator.capacity);
            break;

        default:
            break;
        }
    }
}

// ──────────────────────────────────────────────────────────
// fillDebugInfo — DebugInfo 채우기 (render 직전)
// ──────────────────────────────────────────────────────────

void Bootstrapper::fillDebugInfo(RenderFrame& frame, int realDeltaMs) {
    const GameTime t = simClock_.currentGameTime();
    const float fps  = (realDeltaMs > 0) ? 1000.0f / static_cast<float>(realDeltaMs) : 0.0f;

    auto& d          = frame.debug;
    d.gameTick       = static_cast<int>(simClock_.currentTick());
    d.gameDay        = t.day;     // 0-indexed (UI 표시 시 +1 — DebugPanel.cpp)
    d.gameHour       = t.hour;
    d.gameMinute     = t.minute;
    d.simSpeed       = static_cast<float>(simClock_.speed());
    d.isPaused       = simClock_.isPaused();
    d.fps            = fps;
    d.elevatorCount  = static_cast<int>(transport_->getAllElevators().size());
    d.npcTotal       = agents_->activeAgentCount();
    d.avgSatisfaction = agents_->getAverageSatisfaction(registry_);

    d.npcIdle    = static_cast<int>(agents_->getAgentsInState(registry_, AgentState::Idle).size());
    d.npcWorking = static_cast<int>(agents_->getAgentsInState(registry_, AgentState::Working).size());
    d.npcResting = static_cast<int>(agents_->getAgentsInState(registry_, AgentState::Resting).size());
}

// ──────────────────────────────────────────────────────────
// shutdown
// ──────────────────────────────────────────────────────────

void Bootstrapper::shutdown() {
    spdlog::info("Tower Tycoon shutting down...");
    sdlRenderer_.shutdown();
    SDL_Quit();
}

} // namespace vse
