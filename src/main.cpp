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

// [PHASE-2] IIoTAdapter: BACnet/IP 연동 예정
// [PHASE-3] INetworkAdapter: 권위 서버 모델 예정

using namespace vse;

static constexpr int   WINDOW_W     = 1280;
static constexpr int   WINDOW_H     = 720;
static constexpr int   TICK_MS      = 100;   // 100ms = 10 TPS
static constexpr int   MAX_TICKS_PER_FRAME = 8;

int main(int /*argc*/, char* /*argv*/[])
{
    spdlog::info("Tower Tycoon starting...");

    // ── SDL2 + Renderer 초기화 ──────────────────────────
    SDLRenderer sdlRenderer;
    if (!sdlRenderer.init(WINDOW_W, WINDOW_H, "Tower Tycoon")) {
        return 1;
    }

    // ── Domain 시스템 조립 ──────────────────────────────
    EventBus      eventBus;
    ConfigManager config;
    config.loadFromFile("assets/config/game_config.json");

    entt::registry  registry;
    GridSystem      grid(eventBus, config);
    AgentSystem     agents(grid, eventBus);
    TransportSystem transport(grid, eventBus, config);

    // 초기 건물 설정 — 5층 빌드 + 엘리베이터 1축 2대
    for (int f = 1; f <= 4; ++f) grid.buildFloor(f);
    grid.placeElevatorShaft(5, 0, 4);
    transport.createElevator(5, 0, 4, 8);
    transport.createElevator(5, 0, 2, 4);   // 2대째 (범위 다름)

    // ── Layer 3 초기화 ──────────────────────────────────
    Camera camera(WINDOW_W, WINDOW_H, 32);
    // 로비 근처에 카메라 중심
    camera.centerOn(
        static_cast<float>(grid.floorWidth() * 32 / 2),
        static_cast<float>(2 * 32)
    );

    InputMapper inputMapper;
    RenderFrameCollector collector(grid, transport);

    // ── 게임 상태 ───────────────────────────────────────
    bool running = true;
    bool paused  = false;
    int  speed   = 1;       // 1x, 2x, 4x
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

        // ── 커맨드 처리 (카메라/시스템) ─────────────────
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
                break;
            default:
                break;
            }
        }

        // ── 2. updateSim ────────────────────────────────
        if (!paused) {
            accumulator += realDeltaMs * speed;
            int ticksThisFrame = 0;

            while (accumulator >= TICK_MS && ticksThisFrame < MAX_TICKS_PER_FRAME) {
                accumulator -= TICK_MS;
                ticksThisFrame++;

                eventBus.flush();

                GameTime time = GameTime::fromTick(currentTick);
                currentTick++;

                // Domain 시스템 업데이트
                agents.update(registry, time);
                transport.update(time);
            }

            // spiral-of-death 방지
            if (accumulator > TICK_MS * MAX_TICKS_PER_FRAME) {
                accumulator = 0;
            }
        }

        // ── 3. render ───────────────────────────────────
        collector.setDrawGrid(drawGrid);
        collector.setDrawDebugInfo(drawDebug);
        RenderFrame frame = collector.collect();
        sdlRenderer.render(frame, camera);
    }

    spdlog::info("Tower Tycoon shutting down...");
    sdlRenderer.shutdown();
    SDL_Quit();
    return 0;
}
