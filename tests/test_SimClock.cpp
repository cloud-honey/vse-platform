#include <catch2/catch_test_macros.hpp>
#include "core/SimClock.h"
#include "core/EventBus.h"
#include <vector>

using namespace vse;

TEST_CASE("SimClock 초기 상태", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    REQUIRE(clock.currentTick() == 0);
    REQUIRE(clock.speed() == 1);
    REQUIRE(clock.isPaused() == false);
}

TEST_CASE("SimClock advanceTick()", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    std::vector<EventType> receivedEvents;
    bus.subscribe(EventType::TickAdvanced, [&](const Event& e) {
        receivedEvents.push_back(e.type);
    });
    
    // advanceTick 호출
    clock.advanceTick();
    
    // flush 전에는 이벤트가 처리되지 않음
    REQUIRE(receivedEvents.empty());
    
    // flush 후 이벤트 처리
    bus.flush();
    REQUIRE(receivedEvents.size() == 1);
    REQUIRE(receivedEvents[0] == EventType::TickAdvanced);
    
    // tick 증가 확인
    REQUIRE(clock.currentTick() == 1);
}

TEST_CASE("SimClock pause/resume", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    std::vector<EventType> receivedEvents;
    bus.subscribe(EventType::TickAdvanced, [&](const Event& e) {
        receivedEvents.push_back(e.type);
    });
    
    // pause 상태에서 advanceTick 호출
    clock.pause();
    REQUIRE(clock.isPaused() == true);
    
    clock.advanceTick();
    bus.flush();
    REQUIRE(receivedEvents.empty()); // 이벤트 발생 안 함
    REQUIRE(clock.currentTick() == 0); // tick 변화 없음
    
    // resume 후 다시 advanceTick
    clock.resume();
    REQUIRE(clock.isPaused() == false);
    
    clock.advanceTick();
    bus.flush();
    REQUIRE(receivedEvents.size() == 1);
    REQUIRE(clock.currentTick() == 1);
}

TEST_CASE("SimClock 속도 배수", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    clock.setSpeed(2);
    REQUIRE(clock.speed() == 2);
    
    // advanceTick은 speed에 관계없이 항상 1 tick만 증가
    clock.advanceTick();
    REQUIRE(clock.currentTick() == 1);
}

TEST_CASE("SimClock HourChanged 이벤트", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    std::vector<EventType> receivedEvents;
    bus.subscribe(EventType::HourChanged, [&](const Event& e) {
        receivedEvents.push_back(e.type);
    });
    
    // 59번 advanceTick (아직 시간 변경 안 됨)
    for (int i = 0; i < 59; ++i) {
        clock.advanceTick();
    }
    bus.flush();
    REQUIRE(receivedEvents.empty());
    
    // 60번째 advanceTick (시간 변경)
    clock.advanceTick();
    bus.flush();
    REQUIRE(receivedEvents.size() == 1);
    REQUIRE(receivedEvents[0] == EventType::HourChanged);
    
    // 시간 확인
    auto gameTime = clock.currentGameTime();
    REQUIRE(gameTime.hour == 1);
    REQUIRE(gameTime.minute == 0);
}

TEST_CASE("SimClock DayChanged 이벤트", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    std::vector<EventType> receivedEvents;
    bus.subscribe(EventType::DayChanged, [&](const Event& e) {
        receivedEvents.push_back(e.type);
    });
    
    // 1439번 advanceTick (아직 날짜 변경 안 됨)
    for (int i = 0; i < 1439; ++i) {
        clock.advanceTick();
    }
    bus.flush();
    REQUIRE(receivedEvents.empty());
    
    // 1440번째 advanceTick (날짜 변경)
    clock.advanceTick();
    bus.flush();
    REQUIRE(receivedEvents.size() == 1);
    REQUIRE(receivedEvents[0] == EventType::DayChanged);
    
    // 날짜 확인
    auto gameTime = clock.currentGameTime();
    REQUIRE(gameTime.day == 1);
    REQUIRE(gameTime.hour == 0);
    REQUIRE(gameTime.minute == 0);
}

TEST_CASE("SimClock GameTime 변환", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    // tick 90 설정 (1시간 30분)
    for (int i = 0; i < 90; ++i) {
        clock.advanceTick();
    }
    
    auto gameTime = clock.currentGameTime();
    REQUIRE(gameTime.day == 0);
    REQUIRE(gameTime.hour == 1);
    REQUIRE(gameTime.minute == 30);
}

TEST_CASE("SimClock restoreState", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    // 일부 진행
    for (int i = 0; i < 100; ++i) {
        clock.advanceTick();
    }
    clock.setSpeed(2);
    clock.pause();
    
    // 상태 저장
    SimTick savedTick = clock.currentTick();
    int savedSpeed = clock.speed();
    bool savedPaused = clock.isPaused();
    
    // 더 진행
    clock.resume();
    for (int i = 0; i < 50; ++i) {
        clock.advanceTick();
    }
    
    // 상태 복원
    clock.restoreState(savedTick, savedSpeed, savedPaused);
    
    REQUIRE(clock.currentTick() == savedTick);
    REQUIRE(clock.speed() == savedSpeed);
    REQUIRE(clock.isPaused() == savedPaused);
}

TEST_CASE("EventBus deferred publish", "[EventBus]") {
    EventBus bus;
    
    int handlerCallCount = 0;
    bus.subscribe(EventType::TickAdvanced, [&](const Event&) {
        handlerCallCount++;
    });
    
    // publish 직후
    Event event;
    event.type = EventType::TickAdvanced;
    bus.publish(event);
    
    REQUIRE(handlerCallCount == 0); // 아직 호출 안 됨
    
    // flush 후
    bus.flush();
    REQUIRE(handlerCallCount == 1); // 호출됨
}

TEST_CASE("EventBus unsubscribe", "[EventBus]") {
    EventBus bus;
    
    int handlerCallCount = 0;
    auto id = bus.subscribe(EventType::TickAdvanced, [&](const Event&) {
        handlerCallCount++;
    });
    
    // 구독자 수 확인
    REQUIRE(bus.subscriberCount(EventType::TickAdvanced) == 1);
    
    // 구독 취소
    bus.unsubscribe(id);
    
    // 구독자 수 확인
    REQUIRE(bus.subscriberCount(EventType::TickAdvanced) == 0);
    
    // 이벤트 발행 및 flush
    Event event;
    event.type = EventType::TickAdvanced;
    bus.publish(event);
    bus.flush();
    
    REQUIRE(handlerCallCount == 0); // 핸들러 호출 안 됨
}

TEST_CASE("EventBus pendingCount", "[EventBus]") {
    EventBus bus;
    
    REQUIRE(bus.pendingCount() == 0);
    
    Event event;
    event.type = EventType::TickAdvanced;
    bus.publish(event);
    
    REQUIRE(bus.pendingCount() == 1);
    
    bus.flush();
    REQUIRE(bus.pendingCount() == 0);
}

TEST_CASE("SimClock restoreState silent", "[SimClock]") {
    EventBus bus;
    SimClock clock(bus);
    
    int hourCount = 0;
    int dayCount = 0;
    bus.subscribe(EventType::HourChanged, [&](const Event&) { hourCount++; });
    bus.subscribe(EventType::DayChanged, [&](const Event&) { dayCount++; });
    
    // 1일 1시간 지점으로 복원
    clock.restoreState(1500, 1, false);
    bus.flush();
    
    REQUIRE(hourCount == 0);
    REQUIRE(dayCount == 0);
    REQUIRE(bus.pendingCount() == 0);
}

TEST_CASE("EventBus publish during flush", "[EventBus]") {
    EventBus bus;
    
    int firstCount = 0;
    int secondCount = 0;
    
    bus.subscribe(EventType::TickAdvanced, [&](const Event&) {
        firstCount++;
        // flush 중에 새 이벤트 발행
        bus.publish(Event{EventType::HourChanged});
    });
    bus.subscribe(EventType::HourChanged, [&](const Event&) {
        secondCount++;
    });
    
    bus.publish(Event{EventType::TickAdvanced});
    bus.flush(); // TickAdvanced 처리 → HourChanged 큐에 추가
    
    REQUIRE(firstCount == 1);
    REQUIRE(secondCount == 0); // 현재 flush에서는 미처리
    
    bus.flush(); // HourChanged 처리
    REQUIRE(secondCount == 1);
}