#pragma once
#include "core/IRenderCommand.h"

// Forward declarations — SDL2/ImGui 타입 (헤더에 include 금지)
struct SDL_Window;
struct SDL_Renderer;

namespace vse {

/**
 * DebugPanel — Dear ImGui 디버그 패널.
 *
 * SDLRenderer와 분리된 독립 구성요소 (Design Spec §디렉터리 구조).
 * SDLRenderer가 소유하며, ImGui context 초기화 이후 사용 가능.
 *
 * 책임:
 * - DebugInfo 기반 패널 렌더링
 * - 패널 표시 여부 토글 (F3 단축키 연동은 Bootstrapper에서)
 * - SDLRenderer 없이 단독 사용 불가 (ImGui context 필요)
 *
 * 레이어: Layer 3 — Domain/Core 참조 금지.
 */
class DebugPanel {
public:
    DebugPanel() = default;
    ~DebugPanel() = default;

    // 비복사
    DebugPanel(const DebugPanel&) = delete;
    DebugPanel& operator=(const DebugPanel&) = delete;

    /**
     * 패널 그리기.
     * ImGui NewFrame() 이후, Render() 이전에 호출해야 함.
     * @param frame  현재 렌더 프레임 (debug 필드 사용)
     */
    void draw(const RenderFrame& frame) const;

    // 표시 여부 토글 (F3 단축키용)
    void toggle()          { visible_ = !visible_; }
    void setVisible(bool v){ visible_ = v; }
    bool isVisible() const { return visible_; }

private:
    bool visible_ = true;
};

} // namespace vse
