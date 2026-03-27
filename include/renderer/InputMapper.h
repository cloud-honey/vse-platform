#pragma once
#include "core/InputTypes.h"
#include "renderer/BuildModeState.h"
#include "renderer/Camera.h"
#include <SDL.h>
#include <vector>

namespace vse {

/**
 * InputMapper — SDL_Event를 GameCommand로 변환.
 *
 * Layer 3 → Domain 경계. Domain은 SDL_Event를 직접 보지 않는다.
 *
 * Phase 1 매핑:
 * - 방향키 / WASD: 카메라 팬
 * - 마우스 휠: 줌
 * - Space: 일시정지 토글
 * - 1/2/4: 배속 설정
 * - F3: 디버그 오버레이 토글
 * - ESC: 종료
 * - 마우스 클릭: 타일 선택
 * - B/T 키: 건설 모드 토글
 */
class InputMapper {
public:
    InputMapper() = default;

    // SDL_Event를 처리하여 GameCommand 목록 반환
    // 반환된 커맨드는 commandQueue에 추가된다
    void processEvent(const SDL_Event& event, std::vector<GameCommand>& outCommands);

    // 키 지속 입력 (매 프레임 호출)
    void processHeldKeys(std::vector<GameCommand>& outCommands);

    // 카메라 팬 속도 (px/frame)
    void setPanSpeed(float speed) { panSpeed_ = speed; }
    
    // 줌 스텝 설정
    void setZoomStep(float step) { zoomStep_ = step; }

    // Build mode management
    void setCamera(const Camera* cam) { camera_ = cam; }
    void setBuildMode(BuildModeState state) { buildMode_ = state; }
    BuildModeState getBuildMode() const { return buildMode_; }
    
    // Popup control
    bool shouldOpenTenantPopup() const { return openTenantPopup_; }
    void clearTenantPopupFlag() { openTenantPopup_ = false; }

private:
    float panSpeed_ = 8.0f;
    float zoomStep_ = 0.15f;
    const Camera* camera_ = nullptr;
    BuildModeState buildMode_;
    bool openTenantPopup_ = false;
    
    // Mouse state tracking for drag pan
    bool rightMouseDown_ = false;
    bool middleMouseDown_ = false;
    int lastMouseX_ = 0;
    int lastMouseY_ = 0;
};

} // namespace vse
