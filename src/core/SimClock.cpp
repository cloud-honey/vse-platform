#include "core/SimClock.h"
#include "core/EventBus.h"
#include <cassert>

namespace vse {

SimClock::SimClock(EventBus& bus) : eventBus_(bus) {}

void SimClock::advanceTick() {
    if (paused_) {
        return;
    }

    SimTick oldTick = tick_;
    tick_ += 1;
    
    checkTimeBoundaries(oldTick, tick_);
    
    // 항상 TickAdvanced 이벤트 발행
    Event tickEvent;
    tickEvent.type = EventType::TickAdvanced;
    eventBus_.publish(tickEvent);
}

void SimClock::pause() {
    paused_ = true;
}

void SimClock::resume() {
    paused_ = false;
}

void SimClock::setSpeed(int multiplier) {
    // 1x, 2x, 4x만 허용
    assert(multiplier == 1 || multiplier == 2 || multiplier == 4);
    if (multiplier != 1 && multiplier != 2 && multiplier != 4) {
        return; // release 빌드 안전망
    }
    speed_ = multiplier;
}

SimTick SimClock::currentTick() const {
    return tick_;
}

GameTime SimClock::currentGameTime() const {
    return GameTime::fromTick(tick_);
}

int SimClock::speed() const {
    return speed_;
}

bool SimClock::isPaused() const {
    return paused_;
}

void SimClock::restoreState(SimTick tick, int speed, bool paused) {
    // Silent restore — 이벤트 발행 없음
    // 세이브 로드는 "상태 복원"이지 "시간 경과"가 아님
    tick_ = tick;
    speed_ = speed;
    paused_ = paused;
}

void SimClock::checkTimeBoundaries(SimTick oldTick, SimTick newTick) {
    // 이전 틱과 새 틱 사이의 경계 체크
    int oldTotalMin = static_cast<int>(oldTick);
    int newTotalMin = static_cast<int>(newTick);
    
    // HourChanged 체크 (60분마다)
    int oldHour = (oldTotalMin % 1440) / 60;
    int newHour = (newTotalMin % 1440) / 60;
    if (oldHour != newHour) {
        Event hourEvent;
        hourEvent.type = EventType::HourChanged;
        hourEvent.payload = newHour;  // 현재 시간(0-23)을 페이로드에 포함
        eventBus_.publish(hourEvent);
    }
    
    // DayChanged 체크 (1440분마다)
    int oldDay = oldTotalMin / 1440;
    int newDay = newTotalMin / 1440;
    if (oldDay != newDay) {
        Event dayEvent;
        dayEvent.type = EventType::DayChanged;
        eventBus_.publish(dayEvent);
    }
}

} // namespace vse