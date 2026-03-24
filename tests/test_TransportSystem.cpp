#include "domain/TransportSystem.h"
#include "domain/GridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <catch2/catch_test_macros.hpp>

using namespace vse;

static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

struct TFixture {
    EventBus      bus;
    ConfigManager cfg;
    GridSystem    grid;
    TransportSystem transport;

    TFixture() : grid(bus, cfg), transport(grid, bus, cfg) {
        cfg.loadFromFile(cfgPath());
        // 층 0~2 건설
        grid.buildFloor(1);
        grid.buildFloor(2);
        // shaft x=5, 0~2층
        grid.placeElevatorShaft(5, 0, 2);
    }
};

// ── 기본 생성 테스트 ──────────────────────────────────────────────────────────

TEST_CASE("TransportSystem - 초기 상태 엘리베이터 없음", "[TransportSystem]") {
    TFixture f;
    REQUIRE(f.transport.getAllElevators().empty() == true);
}

TEST_CASE("TransportSystem - createElevator 성공", "[TransportSystem]") {
    TFixture f;
    auto result = f.transport.createElevator(5, 0, 2, 8);
    REQUIRE(result.ok() == true);
    REQUIRE(result.value != INVALID_ENTITY);
    REQUIRE(f.transport.getAllElevators().size() == 1);
}

TEST_CASE("TransportSystem - createElevator 중복 → 실패", "[TransportSystem]") {
    TFixture f;
    f.transport.createElevator(5, 0, 2, 8);
    auto result = f.transport.createElevator(5, 0, 2, 8);
    REQUIRE(result.ok() == false);
}

TEST_CASE("TransportSystem - createElevator 잘못된 범위 → 실패", "[TransportSystem]") {
    TFixture f;
    // topFloor < bottomFloor
    auto r1 = f.transport.createElevator(5, 2, 0, 8);
    REQUIRE(r1.ok() == false);
    // 음수 capacity
    auto r2 = f.transport.createElevator(5, 0, 2, 0);
    REQUIRE(r2.ok() == false);
}

// ── 초기 상태 확인 ────────────────────────────────────────────────────────────

TEST_CASE("TransportSystem - 생성 직후 Idle 상태", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);
    auto snap = f.transport.getElevatorState(r.value);
    REQUIRE(snap.has_value() == true);
    REQUIRE(snap->state == ElevatorState::Idle);
    REQUIRE(snap->currentFloor == 0);
    REQUIRE(snap->passengerCount == 0);
}

// ── 이동 테스트 ───────────────────────────────────────────────────────────────

TEST_CASE("TransportSystem - callElevator → 이동 시작", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);
    EntityId elevId = r.value;

    // 층 2에서 호출
    f.transport.callElevator(5, 2, ElevatorDirection::Up);

    // 1틱 — Idle → MovingUp
    f.transport.update(GameTime{0, 9, 0});
    auto snap = f.transport.getElevatorState(elevId);
    REQUIRE(snap.has_value() == true);
    REQUIRE(snap->state == ElevatorState::MovingUp);
}

TEST_CASE("TransportSystem - 목적 층 도착 → DoorOpening", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);  // speedTilesPerTick=1
    EntityId elevId = r.value;

    f.transport.callElevator(5, 1, ElevatorDirection::Up);

    // tick1: Idle → MovingUp (currentFloor=0)
    f.transport.update(GameTime{0, 9, 0});
    // tick2: MovingUp → floor 1 도착 → DoorOpening
    f.transport.update(GameTime{0, 9, 0});

    auto snap = f.transport.getElevatorState(elevId);
    REQUIRE(snap.has_value() == true);
    REQUIRE(snap->currentFloor == 1);
    REQUIRE(snap->state == ElevatorState::DoorOpening);
}

TEST_CASE("TransportSystem - DoorOpening → Boarding", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);
    EntityId elevId = r.value;

    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    f.transport.update(GameTime{0, 9, 0}); // Idle → Moving
    f.transport.update(GameTime{0, 9, 0}); // Moving → DoorOpening (도착)
    f.transport.update(GameTime{0, 9, 0}); // DoorOpening → Boarding

    auto snap = f.transport.getElevatorState(elevId);
    REQUIRE(snap->state == ElevatorState::Boarding);
}

// ── 탑승 테스트 ───────────────────────────────────────────────────────────────

TEST_CASE("TransportSystem - boardPassenger 성공", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);
    EntityId elevId = r.value;

    // 엘리베이터를 Boarding 상태로 만들기
    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    f.transport.update(GameTime{0,9,0}); // Moving
    f.transport.update(GameTime{0,9,0}); // DoorOpening
    f.transport.update(GameTime{0,9,0}); // Boarding

    EntityId agent = static_cast<EntityId>(static_cast<entt::entity>(100));
    auto boardResult = f.transport.boardPassenger(elevId, agent, 2);
    REQUIRE(boardResult.ok() == true);

    auto snap = f.transport.getElevatorState(elevId);
    REQUIRE(snap->passengerCount == 1);
}

TEST_CASE("TransportSystem - boardPassenger Boarding 아닐 때 → 실패", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);
    EntityId elevId = r.value;
    // Idle 상태에서 탑승 시도
    EntityId agent = static_cast<EntityId>(static_cast<entt::entity>(100));
    auto result = f.transport.boardPassenger(elevId, agent, 2);
    REQUIRE(result.ok() == false);
}

TEST_CASE("TransportSystem - boardPassenger 정원 초과 → 실패", "[TransportSystem]") {
    TFixture f;
    // capacity=1로 생성
    auto r = f.transport.createElevator(5, 0, 2, 1);
    EntityId elevId = r.value;

    // Boarding 상태로 이동
    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    f.transport.update(GameTime{0,9,0});
    f.transport.update(GameTime{0,9,0});
    f.transport.update(GameTime{0,9,0});

    EntityId a1 = static_cast<EntityId>(static_cast<entt::entity>(101));
    EntityId a2 = static_cast<EntityId>(static_cast<entt::entity>(102));

    f.transport.boardPassenger(elevId, a1, 2);  // 1번 탑승 성공
    auto r2 = f.transport.boardPassenger(elevId, a2, 2);  // 정원 초과
    REQUIRE(r2.ok() == false);
}

TEST_CASE("TransportSystem - exitPassenger", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);
    EntityId elevId = r.value;

    // Boarding 상태로
    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    f.transport.update(GameTime{0,9,0});
    f.transport.update(GameTime{0,9,0});
    f.transport.update(GameTime{0,9,0});

    EntityId agent = static_cast<EntityId>(static_cast<entt::entity>(200));
    f.transport.boardPassenger(elevId, agent, 2);

    auto before = f.transport.getElevatorState(elevId);
    REQUIRE(before->passengerCount == 1);

    f.transport.exitPassenger(elevId, agent);
    auto after = f.transport.getElevatorState(elevId);
    REQUIRE(after->passengerCount == 0);
}

// ── 대기 인원 테스트 ──────────────────────────────────────────────────────────

TEST_CASE("TransportSystem - getWaitingCount", "[TransportSystem]") {
    TFixture f;
    f.transport.createElevator(5, 0, 2, 8);

    REQUIRE(f.transport.getWaitingCount(1) == 0);
    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    REQUIRE(f.transport.getWaitingCount(1) == 1);
}

// ── getElevatorsAtFloor ────────────────────────────────────────────────────────

TEST_CASE("TransportSystem - getElevatorsAtFloor", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);

    // 초기 위치 0층
    auto at0 = f.transport.getElevatorsAtFloor(0);
    REQUIRE(at0.size() == 1);
    REQUIRE(at0[0] == r.value);

    auto at1 = f.transport.getElevatorsAtFloor(1);
    REQUIRE(at1.empty() == true);
}

// ── LOOK 알고리즘 — 방향 우선 ─────────────────────────────────────────────────

TEST_CASE("TransportSystem - LOOK: 위로 이동 중 위쪽 콜 먼저", "[TransportSystem]") {
    TFixture f;
    // 층 0~4 건설
    f.grid.buildFloor(3);
    f.grid.buildFloor(4);
    f.grid.placeElevatorShaft(5, 3, 4);
    auto r = f.transport.createElevator(5, 0, 4, 8);
    EntityId elevId = r.value;

    // 층 1과 층 3 동시 호출 (층 0에서 출발 → 위 방향)
    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    f.transport.callElevator(5, 3, ElevatorDirection::Up);

    // tick1: Idle → MovingUp (nextTarget=1, 가장 가까운 위쪽)
    f.transport.update(GameTime{0,9,0});
    auto snap = f.transport.getElevatorState(elevId);
    REQUIRE(snap->state == ElevatorState::MovingUp);
}
