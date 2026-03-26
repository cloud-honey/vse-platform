#include "renderer/InputMapper.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"

namespace vse {

void InputMapper::processEvent(const SDL_Event& event,
                                std::vector<GameCommand>& outCommands)
{
    // ImGui 이벤트 처리
    ImGui_ImplSDL2_ProcessEvent(&event);
    ImGuiIO& io = ImGui::GetIO();

    // ImGui가 키보드/마우스 이벤트를 캡처하면 게임 커맨드로 변환하지 않음
    if (io.WantCaptureKeyboard && 
        (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)) {
        return;
    }
    if (io.WantCaptureMouse && 
        (event.type == SDL_MOUSEMOTION || 
         event.type == SDL_MOUSEBUTTONDOWN || 
         event.type == SDL_MOUSEBUTTONUP || 
         event.type == SDL_MOUSEWHEEL)) {
        return;
    }

    switch (event.type) {

    case SDL_QUIT:
        outCommands.push_back(GameCommand::makeQuit());
        break;

    case SDL_KEYDOWN:
        if (event.key.repeat) break;  // 리피트 무시

        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            // ESC: 취소 또는 종료
            if (buildMode_.action != BuildAction::None) {
                // Build mode 취소
                buildMode_.action = BuildAction::None;
                buildMode_.active = false;
            } else {
                // 게임 종료
                outCommands.push_back(GameCommand::makeQuit());
            }
            break;
        case SDLK_SPACE:
            outCommands.push_back(GameCommand::makeTogglePause());
            break;
        case SDLK_1:
            outCommands.push_back(GameCommand::makeSetSpeed(1));
            break;
        case SDLK_2:
            outCommands.push_back(GameCommand::makeSetSpeed(2));
            break;
        case SDLK_4:
            outCommands.push_back(GameCommand::makeSetSpeed(4));
            break;
        case SDLK_F3: {
            GameCommand cmd{};
            cmd.type = CommandType::ToggleDebugOverlay;
            outCommands.push_back(cmd);
            break;
        }
        case SDLK_HOME: {
            GameCommand cmd{};
            cmd.type = CommandType::CameraReset;
            outCommands.push_back(cmd);
            break;
        }
        case SDLK_b:
            // B 키: BuildFloor 모드 토글
            if (buildMode_.action == BuildAction::BuildFloor) {
                buildMode_.action = BuildAction::None;
                buildMode_.active = false;
            } else {
                buildMode_.action = BuildAction::BuildFloor;
                buildMode_.active = true;
            }
            break;
        case SDLK_t:
            // T 키: PlaceTenant 모드 토글
            if (buildMode_.action == BuildAction::PlaceTenant) {
                // 이미 PlaceTenant 모드면 tenantType 사이클
                buildMode_.tenantType = (buildMode_.tenantType + 1) % 3;
            } else {
                buildMode_.action = BuildAction::PlaceTenant;
                buildMode_.active = true;
                buildMode_.tenantType = 0; // Office부터 시작
            }
            break;
        default: break;
        }
        break;

    case SDL_MOUSEWHEEL:
        if (event.wheel.y != 0) {
            float delta = event.wheel.y > 0 ? 0.1f : -0.1f;
            outCommands.push_back(GameCommand::makeCameraZoom(delta));
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (camera_ == nullptr) {
                // 카메라가 설정되지 않았으면 기본 SelectTile
                outCommands.push_back(
                    GameCommand::makeSelectTile(event.button.x, event.button.y));
                break;
            }

            // Build mode에 따라 다른 커맨드 생성
            switch (buildMode_.action) {
            case BuildAction::BuildFloor:
                // BuildFloor 커맨드
                if (camera_) {
                    int floor = camera_->screenToTileFloor(event.button.y);
                    outCommands.push_back(GameCommand::makeBuildFloor(floor));
                }
                break;
            case BuildAction::PlaceTenant:
                // PlaceTenant 커맨드 — BuildCursor와 동일하게 중앙 정렬, 우측 경계 클램프
                if (camera_) {
                    int tileX  = camera_->screenToTileX(event.button.x);
                    int floor  = camera_->screenToTileFloor(event.button.y);
                    int startX = tileX - buildMode_.tenantWidth / 2;
                    if (startX < 0) startX = 0;
                    outCommands.push_back(GameCommand::makePlaceTenant(
                        startX, floor, buildMode_.tenantType, buildMode_.tenantWidth));
                }
                break;
            case BuildAction::None:
            default:
                // SelectTile — 픽셀→타일 좌표 변환 후 전달
                {
                    int tx = camera_->screenToTileX(event.button.x);
                    int tf = camera_->screenToTileFloor(event.button.y);
                    outCommands.push_back(GameCommand::makeSelectTile(tx, tf));
                }
                break;
            }
        }
        break;

    default: break;
    }
}

void InputMapper::processHeldKeys(std::vector<GameCommand>& outCommands)
{
    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    float dx = 0, dy = 0;
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) dx -= panSpeed_;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) dx += panSpeed_;
    if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) dy -= panSpeed_;
    if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) dy += panSpeed_;

    if (dx != 0.0f || dy != 0.0f) {
        outCommands.push_back(GameCommand::makeCameraPan(dx, dy));
    }
}

} // namespace vse
