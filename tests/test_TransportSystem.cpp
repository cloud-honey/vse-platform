#include "domain/TransportSystem.h"
#include "domain/GridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <catch2/catch_test_macros.hpp>

using namespace vse;

static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

// ConfigManager를 먼저 로드하기 위한 helper — 멤버 초기화 전 loadFromFile 보장
struct PreloadedConfig : ConfigManager {
    PreloadedConfig() { loadFromFile(cfgPath()); }
};

struct TFixture {
    EventBus         bus;
    PreloadedConfig  cfg;   // 생성자에서 자동 loadFromFile
    GridSystem       grid;
    TransportSystem  transport;

    TFixture() : grid(bus, cfg), transport(grid, bus, cfg) {
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

// ── 추가 테스트 (GPT-5.4 검토 반영) ──────────────────────────────────────────

TEST_CASE("TransportSystem - createElevator 생성 시 ElevatorArrived 미발행", "[TransportSystem]") {
    TFixture f;
    // TFixture 생성자(buildFloor, placeElevatorShaft)가 이미 이벤트를 발행했을 수 있음
    // → createElevator 전후 delta가 0이어야 함 (생성 시 이벤트 추가 없음)
    size_t before = f.bus.pendingCount();
    f.transport.createElevator(5, 0, 2, 8);
    size_t after = f.bus.pendingCount();
    REQUIRE(after == before);  // 생성으로 인한 이벤트 추가 없어야 함
}

TEST_CASE("TransportSystem - 중간 층 연속 정차 (2→8, 3·5 중간 콜)", "[TransportSystem]") {
    TFixture f;
    // 층 0~5 빌드
    for (int i = 3; i <= 5; i++) f.grid.buildFloor(i);
    f.grid.placeElevatorShaft(5, 3, 5);
    auto r = f.transport.createElevator(5, 0, 5, 8);
    EntityId elevId = r.value;

    // 층 5 목적지 콜 + 경유지 3층 콜
    f.transport.callElevator(5, 5, ElevatorDirection::Up);
    f.transport.callElevator(5, 3, ElevatorDirection::Up);

    // tick1: Idle → MovingUp (nextTarget=3, 가까운 위쪽 우선)
    f.transport.update(GameTime{0,9,0});
    auto s1 = f.transport.getElevatorState(elevId);
    REQUIRE(s1->state == ElevatorState::MovingUp);

    // tick2: floor=1, tick3: floor=2, tick4: floor=3 → DoorOpening
    f.transport.update(GameTime{0,9,0}); // floor=1
    f.transport.update(GameTime{0,9,0}); // floor=2
    f.transport.update(GameTime{0,9,0}); // floor=3 → DoorOpening
    auto s4 = f.transport.getElevatorState(elevId);
    REQUIRE(s4->currentFloor == 3);
    REQUIRE(s4->state == ElevatorState::DoorOpening);
}

TEST_CASE("TransportSystem - 반대 방향 콜 보류 (위로 이동 중 Down 콜 나중 처리)", "[TransportSystem]") {
    TFixture f;
    for (int i = 3; i <= 4; i++) f.grid.buildFloor(i);
    f.grid.placeElevatorShaft(5, 3, 4);
    auto r = f.transport.createElevator(5, 0, 4, 8);
    EntityId elevId = r.value;

    // 위쪽 콜 먼저
    f.transport.callElevator(5, 4, ElevatorDirection::Up);
    // 아래쪽 콜 동시 (반대 방향)
    f.transport.callElevator(5, 1, ElevatorDirection::Down);

    // tick1: Idle → MovingUp (위쪽 콜 우선, nextTarget=4)
    f.transport.update(GameTime{0,9,0});
    auto s1 = f.transport.getElevatorState(elevId);
    // 위로 이동 시작했어야 함
    REQUIRE(s1->state == ElevatorState::MovingUp);
}

TEST_CASE("TransportSystem - 같은 shaft 2대 배차", "[TransportSystem]") {
    TFixture f;
    for (int i = 3; i <= 4; i++) f.grid.buildFloor(i);
    f.grid.placeElevatorShaft(5, 3, 4);

    // shaft=5에 2대 생성 (범위가 달라야 중복 체크 통과)
    auto r1 = f.transport.createElevator(5, 0, 2, 8);
    auto r2 = f.transport.createElevator(5, 3, 4, 8);
    REQUIRE(r1.ok());
    REQUIRE(r2.ok());
    REQUIRE(r1.value != r2.value);  // EntityId 충돌 없어야 함
    REQUIRE(f.transport.getAllElevators().size() == 2);

    // 층 1 호출 → car1(0~2층) 담당, car2(3~4층)는 범위 밖
    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    f.transport.update(GameTime{0,9,0});

    // car1이 MovingUp, car2는 Idle 유지
    auto s1 = f.transport.getElevatorState(r1.value);
    auto s2 = f.transport.getElevatorState(r2.value);
    REQUIRE(s1->state == ElevatorState::MovingUp);
    REQUIRE(s2->state == ElevatorState::Idle);
}

TEST_CASE("TransportSystem - DoorClosing 중 같은 층 재호출 → 재오픈", "[TransportSystem]") {
    TFixture f;
    auto r = f.transport.createElevator(5, 0, 2, 8);
    EntityId elevId = r.value;

    // 층 1 도착 → Boarding (doorOpenTicks=3)
    // tick1: Idle → MovingUp
    // tick2: MovingUp → DoorOpening (floor=1 도착)
    // tick3: DoorOpening → Boarding (doorTicks=3)
    // tick4: doorTicks=2, tick5: doorTicks=1, tick6: doorTicks=0 → DoorClosing
    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    for (int i = 0; i < 6; i++) f.transport.update(GameTime{0,9,0});
    auto sc = f.transport.getElevatorState(elevId);
    REQUIRE(sc->state == ElevatorState::DoorClosing);

    // DoorClosing 상태에서 같은 층 재호출 → 다음 tick에 DoorOpening 재전환
    f.transport.callElevator(5, 1, ElevatorDirection::Up);
    f.transport.update(GameTime{0,9,0}); // DoorClosing → 같은 층 콜 감지 → DoorOpening
    auto sr = f.transport.getElevatorState(elevId);
    REQUIRE(sr->state == ElevatorState::DoorOpening);
}

TEST_CASE("TransportSystem - EntityId entt registry 발급 (충돌 없음)", "[TransportSystem]") {
    TFixture f;
    for (int i = 3; i <= 4; i++) f.grid.buildFloor(i);
    f.grid.placeElevatorShaft(5, 3, 4);

    auto r1 = f.transport.createElevator(5, 0, 2, 8);
    auto r2 = f.transport.createElevator(5, 3, 4, 8);
    REQUIRE(r1.ok());
    REQUIRE(r2.ok());
    // entt registry 발급이므로 id가 겹치지 않아야 함
    REQUIRE(r1.value != r2.value);
    REQUIRE(r1.value != INVALID_ENTITY);
    REQUIRE(r2.value != INVALID_ENTITY);
}
