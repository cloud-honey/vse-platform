#pragma once
#include "core/Types.h"
#include <functional>
#include <vector>

namespace vse {

class EventBus;  // forward declaration

class SimClock {
public:
    explicit SimClock(EventBus& bus);

    void advanceTick();         // tick+1, emits TickAdvanced
    void pause();
    void resume();
    void setSpeed(int multiplier);  // 1x, 2x, 4x

    SimTick currentTick() const;
    GameTime currentGameTime() const;
    int speed() const;
    bool isPaused() const;

    void restoreState(SimTick tick, int speed, bool paused);

private:
    EventBus& eventBus_;
    SimTick tick_ = 0;
    int speed_ = 1;
    bool paused_ = false;

    void checkTimeBoundaries(SimTick oldTick, SimTick newTick);
};

} // namespace vse