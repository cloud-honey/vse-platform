#include "renderer/InputMapper.h"

namespace vse {

void InputMapper::processEvent(const SDL_Event& event,
                                std::vector<GameCommand>& outCommands)
{
    switch (event.type) {

    case SDL_QUIT:
        outCommands.push_back(GameCommand::makeQuit());
        break;

    case SDL_KEYDOWN:
        if (event.key.repeat) break;  // 리피트 무시

        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            outCommands.push_back(GameCommand::makeQuit());
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
            outCommands.push_back(
                GameCommand::makeSelectTile(event.button.x, event.button.y));
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
