#include <SDL.h>
#include <spdlog/spdlog.h>

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

// [PHASE-2] IIoTAdapter: BACnet/IP 연동 예정
// [PHASE-3] INetworkAdapter: 권위 서버 모델 예정

using namespace vse;

static constexpr int MAX_TICKS_PER_FRAME = 8;

int main(int /*argc*/, char* /*argv*/[])
{
    spdlog::info("Tower Tycoon starting...");

    // ── Config 로딩 (모든 수치는 여기서) ────────────────
    ConfigManager config;
    config.loadFromFile("assets/config/game_config.json");

    const int windowW     = config.getInt("rendering.windowWidth",  1280);
    const int windowH     = config.getInt("rendering.windowHeight", 720);
    const int tileSizePx  = config.getInt("grid.tileSizePx",        32);
    const int tickMs      = config.getInt("simulation.tickRateMs",  100);
    const float zoomMin   = config.getFloat("rendering.zoomMin",    0.25f);
    const float zoomMax   = config.getFloat("rendering.zoomMax",    4.0f);
    const float panSpeed  = config.getFloat("rendering.cameraPanSpeed", 8.0f);

    // ── SDL2 + Renderer 초기화 ──────────────────────────
    SDLRenderer sdlRenderer;
    if (!sdlRenderer.init(windowW, windowH, "Tower Tycoon")) {
        return 1;
    }

    // ── Domain 시스템 조립 ──────────────────────────────
    EventBus       eventBus;
    entt::registry registry;
    GridSystem      grid(eventBus, config);
    AgentSystem     agents(grid, eventBus);
    TransportSystem transport(grid, eventBus, config);

    // 초기 건물 설정 — 5층 빌드 + 엘리베이터 1축 2대
    for (int f = 1; f <= 4; ++f) grid.buildFloor(f);
    grid.placeElevatorShaft(5, 0, 4);
    transport.createElevator(5, 0, 4, 8);
    transport.createElevator(5, 0, 2, 4);

    // 테넌트 + NPC 배치 (프로토타입 동작 확인용)
    EntityId homeId1 = registry.create();
    EntityId homeId2 = registry.create();
    EntityId homeId3 = registry.create();
    EntityId workId1 = registry.create();
    EntityId workId2 = registry.create();

    grid.placeTenant({0, 0},  TenantType::Residential, 2, homeId1);
    grid.placeTenant({3, 0},  TenantType::Residential, 2, homeId2);
    grid.placeTenant({6, 0},  TenantType::Residential, 2, homeId3);
    grid.placeTenant({0, 2},  TenantType::Office,      3, workId1);
    grid.placeTenant({8, 2},  TenantType::Office,      3, workId2);

    // NPC 5명 스폰
    agents.spawnAgent(registry, homeId1, workId1);
    agents.spawnAgent(registry, homeId2, workId1);
    agents.spawnAgent(registry, homeId3, workId2);
    agents.spawnAgent(registry, homeId1, workId2);
    agents.spawnAgent(registry, homeId2, workId2);

    // ── Layer 3 초기화 (Config 기반) ────────────────────
    Camera camera(windowW, windowH, tileSizePx, zoomMin, zoomMax);
    camera.centerOn(
        static_cast<float>(grid.floorWidth() * tileSizePx / 2),
        static_cast<float>(2 * tileSizePx)
    );

    InputMapper inputMapper;
    inputMapper.setPanSpeed(panSpeed);
    RenderFrameCollector collector(grid, transport, tileSizePx);

    // ── AgentSystem 렌더 연결 (TASK-01-008) ────────────
    collector.setAgentSource(&agents, &registry);

    // ── 게임 상태 ───────────────────────────────────────
    bool running  = true;
    bool paused   = false;
    int  speed    = 1;
    bool drawGrid = true;
    bool drawDebug = true;
    SimTick currentTick = 0;
    Uint32  lastFrameMs = SDL_GetTicks();
    int     accumulator = 0;

    // ── 메인 루프 ───────────────────────────────────────
    while (running) {
        Uint32 nowMs = SDL_GetTicks();
        int realDeltaMs = static_cast<int>(nowMs - lastFrameMs);
        lastFrameMs = nowMs;

        // ── 1. handleInput ──────────────────────────────
        std::vector<GameCommand> commands;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            inputMapper.processEvent(event, commands);
        }
        inputMapper.processHeldKeys(commands);

        // ── 커맨드 처리 (카메라/시스템 + 도메인) ────────
        for (const auto& cmd : commands) {
            switch (cmd.type) {
            case CommandType::Quit:
                running = false;
                break;
            case CommandType::TogglePause:
                paused = !paused;
                spdlog::info("Pause: {}", paused ? "ON" : "OFF");
                break;
            case CommandType::SetSpeed:
                speed = cmd.setSpeed.speedMultiplier;
                spdlog::info("Speed: {}x", speed);
                break;
            case CommandType::CameraPan:
                camera.pan(cmd.cameraPan.deltaX, cmd.cameraPan.deltaY);
                break;
            case CommandType::CameraZoom:
                camera.zoom(cmd.cameraZoom.zoomDelta);
                break;
            case CommandType::CameraReset:
                camera.reset();
                break;
            case CommandType::ToggleDebugOverlay:
                drawDebug = !drawDebug;
                drawGrid  = drawDebug;
                sdlRenderer.debugPanel().setVisible(drawDebug);  // F3 토글 연결
                break;

            // ── 도메인 커맨드 ────────────────────────
            case CommandType::BuildFloor:
                grid.buildFloor(cmd.buildFloor.floor);
                break;
            case CommandType::PlaceTenant:
                grid.placeTenant(
                    TileCoord{cmd.placeTenant.x, cmd.placeTenant.floor},
                    static_cast<TenantType>(cmd.placeTenant.tenantType),
                    cmd.placeTenant.width,
                    INVALID_ENTITY);
                break;
            case CommandType::PlaceElevatorShaft:
                grid.placeElevatorShaft(
                    cmd.placeShaft.shaftX,
                    cmd.placeShaft.bottomFloor,
                    cmd.placeShaft.topFloor);
                break;
            case CommandType::CreateElevator:
                transport.createElevator(
                    cmd.createElevator.shaftX,
                    cmd.createElevator.bottomFloor,
                    cmd.createElevator.topFloor,
                    cmd.createElevator.capacity);
                break;

            default:
                break;
            }
        }

        // ── 2. updateSim ────────────────────────────────
        if (!paused) {
            accumulator += realDeltaMs * speed;
            int ticksThisFrame = 0;

            while (accumulator >= tickMs && ticksThisFrame < MAX_TICKS_PER_FRAME) {
                accumulator -= tickMs;
                ticksThisFrame++;

                eventBus.flush();

                GameTime time = GameTime::fromTick(currentTick);
                currentTick++;

                agents.update(registry, time);
                transport.update(time);
            }

            if (accumulator > tickMs * MAX_TICKS_PER_FRAME) {
                accumulator = 0;
            }
        }

        // ── 3. render ───────────────────────────────────
        collector.setDrawGrid(drawGrid);
        collector.setDrawDebugInfo(drawDebug);
        RenderFrame frame = collector.collect();

        // ── DebugInfo 채우기 (TASK-01-009 통합) ─────────
        // Bootstrapper가 각 시스템에서 데이터 수집 후 주입
        {
            GameTime t = GameTime::fromTick(currentTick);
            float fps  = (realDeltaMs > 0) ? 1000.0f / static_cast<float>(realDeltaMs) : 0.0f;

            auto& d            = frame.debug;
            d.gameTick         = static_cast<int>(currentTick);
            d.gameDay          = t.day;     // 0-indexed (UI 표시 시 +1 — DebugPanel.cpp)
            d.gameHour         = t.hour;
            d.gameMinute       = t.minute;
            d.simSpeed         = static_cast<float>(speed);
            d.isPaused         = paused;
            d.fps              = fps;
            d.elevatorCount    = static_cast<int>(transport.getAllElevators().size());
            d.npcTotal         = agents.activeAgentCount();
            d.avgSatisfaction  = agents.getAverageSatisfaction(registry);

            // 상태별 NPC 카운트
            d.npcIdle    = static_cast<int>(agents.getAgentsInState(registry, AgentState::Idle).size());
            d.npcWorking = static_cast<int>(agents.getAgentsInState(registry, AgentState::Working).size());
            d.npcResting = static_cast<int>(agents.getAgentsInState(registry, AgentState::Resting).size());
        }

        sdlRenderer.render(frame, camera);
    }

    spdlog::info("Tower Tycoon shutting down...");
    sdlRenderer.shutdown();
    SDL_Quit();
    return 0;
}
