#include "core/Bootstrapper.h"
#include <spdlog/spdlog.h>
#include <SDL.h>

namespace vse {

static constexpr int MAX_TICKS_PER_FRAME = 8;

bool Bootstrapper::init() {
    spdlog::info("Tower Tycoon starting...");

    // ── Config 로딩 ─────────────────────────────────────
    config_.loadFromFile("assets/config/game_config.json");
    if (!config_.isLoaded()) {
        spdlog::error("Failed to load config");
        return false;
    }

    windowW_     = config_.getInt("rendering.windowWidth",  1280);
    windowH_     = config_.getInt("rendering.windowHeight", 720);
    tileSizePx_  = config_.getInt("grid.tileSizePx",        32);
    tickMs_      = config_.getInt("simulation.tickRateMs",  100);
    zoomMin_     = config_.getFloat("rendering.zoomMin",    0.25f);
    zoomMax_     = config_.getFloat("rendering.zoomMax",    4.0f);
    panSpeed_    = config_.getFloat("rendering.cameraPanSpeed", 8.0f);

    // ── SDL2 + Renderer 초기화 ──────────────────────────
    if (!sdlRenderer_.init(windowW_, windowH_, "Tower Tycoon")) {
        return false;
    }

    // ── Domain 시스템 조립 ──────────────────────────────
    grid_ = std::make_unique<GridSystem>(eventBus_, config_);
    agents_ = std::make_unique<AgentSystem>(*grid_, eventBus_);
    transport_ = std::make_unique<TransportSystem>(*grid_, eventBus_, config_);

    // 초기 건물 설정 — 5층 빌드 + 엘리베이터 1축 2대
    for (int f = 1; f <= 4; ++f) grid_->buildFloor(f);
    grid_->placeElevatorShaft(5, 0, 4);
    transport_->createElevator(5, 0, 4, 8);
    transport_->createElevator(5, 0, 2, 4);

    // 테넌트 + NPC 배치 (프로토타입 동작 확인용)
    EntityId homeId1 = registry_.create();
    EntityId homeId2 = registry_.create();
    EntityId homeId3 = registry_.create();
    EntityId workId1 = registry_.create();
    EntityId workId2 = registry_.create();

    grid_->placeTenant({0, 0},  TenantType::Residential, 2, homeId1);
    grid_->placeTenant({3, 0},  TenantType::Residential, 2, homeId2);
    grid_->placeTenant({6, 0},  TenantType::Residential, 2, homeId3);
    grid_->placeTenant({0, 2},  TenantType::Office,      3, workId1);
    grid_->placeTenant({8, 2},  TenantType::Office,      3, workId2);

    // NPC 5명 스폰
    agents_->spawnAgent(registry_, homeId1, workId1);
    agents_->spawnAgent(registry_, homeId2, workId1);
    agents_->spawnAgent(registry_, homeId3, workId2);
    agents_->spawnAgent(registry_, homeId1, workId2);
    agents_->spawnAgent(registry_, homeId2, workId2);

    // ── Layer 3 초기화 (Config 기반) ────────────────────
    camera_ = Camera(windowW_, windowH_, tileSizePx_, zoomMin_, zoomMax_);
    camera_.centerOn(
        static_cast<float>(grid_->floorWidth() * tileSizePx_ / 2),
        static_cast<float>(2 * tileSizePx_)
    );

    inputMapper_.setPanSpeed(panSpeed_);
    // RenderFrameCollector는 참조 멤버가 있어서 할당 불가 - unique_ptr로 관리
    collector_ = std::make_unique<RenderFrameCollector>(*grid_, *transport_, tileSizePx_);

    // ── AgentSystem 렌더 연결 (TASK-01-008) ────────────
    collector_->setAgentSource(agents_.get(), &registry_);

    running_ = true;
    drawGrid_ = true;
    drawDebug_ = true;
    
    spdlog::info("Bootstrapper initialized successfully");
    return true;
}

bool Bootstrapper::initDomainOnly(const std::string& configPath) {
    // Config 로딩 — 경로가 "assets/"로 시작하면 VSE_PROJECT_ROOT 기준 절대 경로로 변환
    std::string resolvedPath = configPath;
    if (configPath.rfind("assets/", 0) == 0) {
        resolvedPath = std::string(VSE_PROJECT_ROOT) + "/" + configPath;
    }
    config_.loadFromFile(resolvedPath);
    if (!config_.isLoaded()) {
        spdlog::error("Failed to load config from {}", configPath);
        return false;
    }

    tileSizePx_ = config_.getInt("grid.tileSizePx", 32);
    tickMs_ = config_.getInt("simulation.tickRateMs", 100);

    // Domain 시스템 조립 (SDL 없이)
    grid_ = std::make_unique<GridSystem>(eventBus_, config_);
    agents_ = std::make_unique<AgentSystem>(*grid_, eventBus_);
    transport_ = std::make_unique<TransportSystem>(*grid_, eventBus_, config_);

    // 초기 건물 설정 (최소한의 초기화)
    for (int f = 1; f <= 4; ++f) grid_->buildFloor(f);
    grid_->placeElevatorShaft(5, 0, 4);
    transport_->createElevator(5, 0, 4, 8);
    transport_->createElevator(5, 0, 2, 4);

    // 기본 테넌트 배치
    EntityId homeId1 = registry_.create();
    EntityId workId1 = registry_.create();
    grid_->placeTenant({0, 0}, TenantType::Residential, 2, homeId1);
    grid_->placeTenant({0, 2}, TenantType::Office, 3, workId1);

    // NPC 1명 스폰
    agents_->spawnAgent(registry_, homeId1, workId1);

    running_ = true;
    
    spdlog::info("Domain-only initialization completed");
    return true;
}

void Bootstrapper::run() {
    Uint32 lastFrameMs = SDL_GetTicks();
    
    while (running_) {
        Uint32 nowMs = SDL_GetTicks();
        int realDeltaMs = static_cast<int>(nowMs - lastFrameMs);
        lastFrameMs = nowMs;

        // ── 1. handleInput ──────────────────────────────
        std::vector<GameCommand> commands;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            inputMapper_.processEvent(event, commands);
        }
        inputMapper_.processHeldKeys(commands);

        // ── 커맨드 처리 ────────────────────────────────
        processCommands(commands, running_);

        // ── 2. updateSim ────────────────────────────────
        if (!paused_) {
            accumulator_ += realDeltaMs * speed_;
            int ticksThisFrame = 0;

            while (accumulator_ >= tickMs_ && ticksThisFrame < MAX_TICKS_PER_FRAME) {
                accumulator_ -= tickMs_;
                ticksThisFrame++;

                eventBus_.flush();

                GameTime time = GameTime::fromTick(currentTick_);
                currentTick_++;

                if (ticksThisFrame == 1) {
                    // 첫 틱에서만 커맨드 처리 (원본 main.cpp와 동일)
                    // 이미 processCommands에서 처리했으므로 여기서는 추가 처리 없음
                }

                agents_->update(registry_, time);
                transport_->update(time);
            }

            if (accumulator_ > tickMs_ * MAX_TICKS_PER_FRAME) {
                accumulator_ = 0;
            }
        }

        // ── 3. render ───────────────────────────────────
        collector_->setDrawGrid(drawGrid_);
        collector_->setDrawDebugInfo(drawDebug_);
        RenderFrame frame = collector_->collect();

        // ── DebugInfo 채우기 ────────────────────────────
        fillDebugInfo(frame, realDeltaMs);

        sdlRenderer_.render(frame, camera_);
    }
}

void Bootstrapper::processCommands(const std::vector<GameCommand>& cmds, bool& running) {
    for (const auto& cmd : cmds) {
        switch (cmd.type) {
        case CommandType::Quit:
            running = false;
            running_ = false;
            break;
        case CommandType::TogglePause:
            paused_ = !paused_;
            spdlog::info("Pause: {}", paused_ ? "ON" : "OFF");
            break;
        case CommandType::SetSpeed:
            speed_ = cmd.setSpeed.speedMultiplier;
            spdlog::info("Speed: {}x", speed_);
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
            drawGrid_ = drawDebug_;
            sdlRenderer_.debugPanel().setVisible(drawDebug_);
            break;
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

void Bootstrapper::fillDebugInfo(RenderFrame& frame, int realDeltaMs) {
    GameTime t = GameTime::fromTick(currentTick_);
    float fps  = (realDeltaMs > 0) ? 1000.0f / static_cast<float>(realDeltaMs) : 0.0f;

    auto& d            = frame.debug;
    d.gameTick         = static_cast<int>(currentTick_);
    d.gameDay          = t.day;     // 0-indexed (UI 표시 시 +1 — DebugPanel.cpp)
    d.gameHour         = t.hour;
    d.gameMinute       = t.minute;
    d.simSpeed         = static_cast<float>(speed_);
    d.isPaused         = paused_;
    d.fps              = fps;
    d.elevatorCount    = static_cast<int>(transport_->getAllElevators().size());
    d.npcTotal         = agents_->activeAgentCount();
    d.avgSatisfaction  = agents_->getAverageSatisfaction(registry_);

    // 상태별 NPC 카운트
    d.npcIdle    = static_cast<int>(agents_->getAgentsInState(registry_, AgentState::Idle).size());
    d.npcWorking = static_cast<int>(agents_->getAgentsInState(registry_, AgentState::Working).size());
    d.npcResting = static_cast<int>(agents_->getAgentsInState(registry_, AgentState::Resting).size());
}

void Bootstrapper::shutdown() {
    spdlog::info("Tower Tycoon shutting down...");
    sdlRenderer_.shutdown();
    SDL_Quit();
}

} // namespace vse