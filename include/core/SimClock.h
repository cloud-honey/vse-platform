#pragma once
#include "core/Types.h"
#include <functional>
#include <vector>

namespace vse {

class EventBus;  // forward declaration

/**
 * SimClock — 시뮬레이션 틱 카운터 및 게임 시간 변환기.
 *
 * 설계 원칙:
 *   - 1 tick = 1 game minute (Phase 1 하드코딩)
 *   - 1440 ticks = 1 game day (24h × 60min)
 *   - 실제 시간 100ms = 1 sim tick (Bootstrapper의 accumulator가 관리)
 *
 * 배속 처리:
 *   - advanceTick()은 항상 정확히 1 tick만 진행
 *   - 배속(1x/2x/4x)은 Bootstrapper가 루프 횟수로 조절
 *   - setSpeed()는 Bootstrapper가 accumulator 계산에 사용할 배수만 저장
 *
 * 이벤트 발행 (EventBus 통해 deferred):
 *   - TickAdvanced : 매 advanceTick() 호출 시
 *   - HourChanged  : 60 tick마다 (game minute → hour 경계)
 *   - DayChanged   : 1440 tick마다 (game day 경계)
 *
 * Save/Load:
 *   - restoreState()는 silent restore — 이벤트 발행 없음
 *   - "상태 복원"이지 "시간 경과"가 아니므로 경계 이벤트 미발행
 */
class SimClock {
public:
    explicit SimClock(EventBus& bus);

    // 틱 진행 — 항상 1 tick만 증가. paused 시 무동작.
    // TickAdvanced 이벤트 발행 (deferred)
    void advanceTick();

    void pause();
    void resume();

    // 배속 설정 — 1x/2x/4x만 허용.
    // assert + runtime guard: 잘못된 값은 무시됨.
    void setSpeed(int multiplier);

    SimTick  currentTick()     const;
    GameTime currentGameTime() const;  // tick에서 GameTime으로 변환
    int      speed()           const;
    bool     isPaused()        const;

    // 세이브 로드용 상태 복원 — 이벤트 발행 없음 (silent restore)
    void restoreState(SimTick tick, int speed, bool paused);

private:
    EventBus& eventBus_;
    SimTick tick_  = 0;
    int     speed_ = 1;
    bool    paused_ = false;

    // tick 경계(HourChanged, DayChanged) 감지 후 이벤트 발행
    void checkTimeBoundaries(SimTick oldTick, SimTick newTick);
};

} // namespace vse
