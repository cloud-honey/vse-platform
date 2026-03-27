/**
 * @file DayNightCycle.cpp
 * @brief 게임 시간에 따른 주야간 시각적 사이클 구현.
 */
#include "renderer/DayNightCycle.h"
#include "core/SimClock.h"
#include <spdlog/spdlog.h>

namespace vse {

DayNightCycle::DayNightCycle(EventBus& bus)
    : bus_(bus)
{
    // HourChanged 이벤트 구독
    subscriptionId_ = bus_.subscribe(EventType::HourChanged, 
        [this](const Event& e) { this->onHourChanged(e); });
    
    spdlog::debug("DayNightCycle initialized");
}

DayNightCycle::~DayNightCycle() {
    // 구독 해제
    if (subscriptionId_ != 0) {
        bus_.unsubscribe(subscriptionId_);
        subscriptionId_ = 0;
    }
    
    spdlog::debug("DayNightCycle destroyed");
}

void DayNightCycle::onHourChanged(const Event& e) {
    // 페이로드에서 시간 추출 (예외 없이 안전하게)
    if (e.payload.has_value()) {
        // std::any_cast without exceptions - check type first
        const std::type_info& type = e.payload.type();
        if (type == typeid(int)) {
            int hour = *std::any_cast<int>(&e.payload);
            if (hour >= 0 && hour < 24) {
                currentHour_ = hour;
                spdlog::debug("DayNightCycle: hour changed to {}", currentHour_);
            }
        } else {
            spdlog::warn("DayNightCycle: event payload is not int type");
        }
    }
}

SDL_Color DayNightCycle::getOverlayColor() const {
    // 시간대별 오버레이 색상
    if (currentHour_ >= 0 && currentHour_ <= 5) {
        // Night (0-5): dark blue tint
        return SDL_Color{0, 0, 80, 120};
    } else if (currentHour_ >= 6 && currentHour_ <= 7) {
        // Dawn (6-7): orange tint
        return SDL_Color{255, 140, 0, 60};
    } else if (currentHour_ >= 8 && currentHour_ <= 17) {
        // Day (8-17): no overlay
        return SDL_Color{0, 0, 0, 0};
    } else if (currentHour_ >= 18 && currentHour_ <= 19) {
        // Dusk (18-19): orange tint
        return SDL_Color{255, 100, 0, 80};
    } else {
        // Evening (20-23): dark tint
        return SDL_Color{0, 0, 60, 100};
    }
}

} // namespace vse