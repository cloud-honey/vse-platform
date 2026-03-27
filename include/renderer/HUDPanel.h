#pragma once
#include "core/IRenderCommand.h"
#include <vector>
#include <string>

namespace vse {

// Toast system
struct ToastMessage {
    std::string text;
    float remainingSeconds;
    enum class Type { Info, Warning, Error } type = Type::Info;
};

/**
 * HUDPanel — 게임 HUD (잔액, 별점, 틱, 테넌트/NPC 수).
 *
 * SDLRenderer와 분리된 독립 구성요소 (Design Spec §디렉터리 구조).
 * SDLRenderer가 소유하며, ImGui context 초기화 이후 사용 가능.
 *
 * 책임:
 * - RenderFrame의 HUD 데이터 기반 패널 렌더링
 * - 패널 표시 여부 토글
 * - SDLRenderer 없이 단독 사용 불가 (ImGui context 필요)
 *
 * 레이어: Layer 3 — Domain/Core 참조 금지.
 */
class HUDPanel {
public:
    HUDPanel() = default;
    ~HUDPanel() = default;

    // 비복사
    HUDPanel(const HUDPanel&) = delete;
    HUDPanel& operator=(const HUDPanel&) = delete;

    /**
     * HUD 패널 그리기.
     * ImGui NewFrame() 이후, Render() 이전에 호출해야 함.
     * @param frame  현재 렌더 프레임 (HUD 필드 사용)
     */
    void draw(const RenderFrame& frame);

    // 표시 여부 토글
    void toggle()          { visible_ = !visible_; }
    void setVisible(bool v){ visible_ = v; }
    bool isVisible() const { return visible_; }

    // 포맷팅 헬퍼 (테스트 접근 가능하도록 public static)
    static std::string formatBalance(int64_t balance); // e.g. "₩1,234,567"
    static std::string formatStars(float rating);      // e.g. "★★★☆☆ (3.2)"

    /// Push a toast notification (max MAX_TOASTS; oldest dismissed on overflow)
    void pushToast(const std::string& text, ToastMessage::Type type = ToastMessage::Type::Info);

    /// Draw speed control buttons. Returns speed multiplier if changed, -1 if unchanged.
    int drawSpeedButtons(int currentSpeed);

    /// Draw construction toolbar at bottom of screen.
    /// Returns BuildAction::None / BuildFloor / PlaceTenant(type) based on button click.
    /// outTenantType: set to 0/1/2 if PlaceTenant selected.
    int drawToolbar(int viewportH);  // returns 0=none, 1=floor, 2=office, 3=residential, 4=commercial

    static constexpr int MAX_TOASTS = 3;
    static constexpr float TOAST_DURATION = 3.0f;  ///< seconds

private:
    bool visible_ = true;
    std::vector<ToastMessage> toasts_;
    float lastFrameTime_ = 0.0f;  ///< For toast timer delta

    void drawToasts();
    void updateToasts(float dt);
    std::string formatDailyChange(int64_t change) const;
};

} // namespace vse