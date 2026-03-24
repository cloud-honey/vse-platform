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
    tick_ += speed_;
    
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
    SimTick oldTick = tick_;
    tick_ = tick;
    speed_ = speed;
    paused_ = paused;
    
    checkTimeBoundaries(oldTick, tick_);
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