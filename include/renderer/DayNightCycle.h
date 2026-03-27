#pragma once
#include "core/EventBus.h"
#include <SDL.h>
#include <cstdint>

namespace vse {

/**
 * DayNightCycle — 게임 시간에 따른 주야간 시각적 사이클.
 * 
 * SimClock의 HourChanged 이벤트를 구독하여 현재 시간을 추적하고,
 * 시간대별 오버레이 색상을 제공한다.
 */
class DayNightCycle {
public:
    explicit DayNightCycle(EventBus& bus);
    ~DayNightCycle();

    // 복사/이동 금지 — this 캡처 람다 + subscriptionId_ 때문에 안전하지 않음
    DayNightCycle(const DayNightCycle&) = delete;
    DayNightCycle& operator=(const DayNightCycle&) = delete;
    DayNightCycle(DayNightCycle&&) = delete;
    DayNightCycle& operator=(DayNightCycle&&) = delete;

    // 현재 게임 시간(0-23)에 따른 RGBA 오버레이 색상 반환
    // Night(0-5):   dark blue tint  SDL_Color{0,0,80,120}
    // Dawn(6-7):    orange tint     SDL_Color{255,140,0,60}
    // Day(8-17):    no overlay      SDL_Color{0,0,0,0}
    // Dusk(18-19):  orange tint     SDL_Color{255,100,0,80}
    // Evening(20-23): dark tint     SDL_Color{0,0,60,100}
    SDL_Color getOverlayColor() const;

    // 현재 시간 반환 (0-23)
    int currentHour() const { return currentHour_; }

private:
    EventBus& bus_;
    int currentHour_ = 8;  // 기본값: 낮 시간 (8시)
    
    // HourChanged 이벤트 구독 ID
    uint32_t subscriptionId_ = 0;
    
    // 이벤트 핸들러
    void onHourChanged(const Event& e);
};

} // namespace vse