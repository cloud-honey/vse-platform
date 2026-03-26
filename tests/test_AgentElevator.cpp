#include "domain/AgentSystem.h"
#include "domain/GridSystem.h"
#include "domain/TransportSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

using namespace vse;

static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

struct PreloadedConfig : ConfigManager {
    PreloadedConfig() { loadFromFile(cfgPath()); }
};

// ── 엘리베이터 포함 테스트 픽스처 ────────────────────────────────────────────
// 층 구성:
//   floor 0: 거주지 (homeTenant, x=0~1)
//   floor 6: 직장   (workTenant, x=0~1)
//   elevator shaft x=0, floor 0~6
struct ElevFixture {
    EventBus         bus;
    PreloadedConfig  cfg;
    GridSystem       grid;
    TransportSystem  transport;
    AgentSystem      agents;
    entt::registry   reg;

    EntityId homeTenantId = INVALID_ENTITY;
    EntityId workTenantId = INVALID_ENTITY;
    EntityId elevId       = INVALID_ENTITY;

    ElevFixture()
        : grid(bus, cfg)
        , transport(grid, bus, cfg)
        , agents(grid, bus, transport)
    {
        // 층 0 (auto), 1, 2 건설
        for (int f = 1; f <= 6; ++f) grid.buildFloor(f);

        // elevator shaft x=0, floor 0~6
        grid.placeElevatorShaft(0, 0, 6);
        auto elevResult = transport.createElevator(0, 0, 6, 8);
        REQUIRE(elevResult.ok());
        elevId = elevResult.value;

        // 거주지 테넌트 floor 0, x=2 (shaft 피하기 위해 x=2 사용)
        homeTenantId = reg.create();
        grid.placeTenant({2, 0}, TenantType::Residential, 2, homeTenantId);

        // 직장 테넌트 floor 6, x=2
        workTenantId = reg.create();
        grid.placeTenant({2, 6}, TenantType::Office, 2, workTenantId);
    }

    // 엘리베이터를 특정 층의 Boarding 상태로 만드는 헬퍼
    // 층 0에서 targetFloor까지 이동 (층 0 출발 기준)
    void driveElevatorToBoarding(int targetFloor) {
        // callElevator로 목적지 층 호출
        transport.callElevator(0, targetFloor, targetFloor > 0
                                               ? ElevatorDirection::Up
                                               : ElevatorDirection::Down);
        // tick: Idle → Moving (1틱)
        transport.update(GameTime{0, 9, 0});
        // tick: Moving → arrive → DoorOpening (targetFloor 틱)
        for (int i = 0; i < targetFloor; ++i) {
            transport.update(GameTime{0, 9, 0});
        }
        // tick: DoorOpening → Boarding (1틱)
        transport.update(GameTime{0, 9, 0});

        auto snap = transport.getElevatorState(elevId);
        // DoorOpening 혹은 Boarding이면 OK
        REQUIRE(snap.has_value());
        REQUIRE((snap->state == ElevatorState::Boarding ||
                 snap->state == ElevatorState::DoorOpening));
    }
};

// ── Test 1: floor 0 거주 NPC → floor 6 직장 → tick 540 (9am) → WaitingElevator ──

TEST_CASE("AgentElevator - NPC floor 0, workplace floor 6 → WaitingElevator at 9am", "[AgentElevator]") {
    ElevFixture f;

    // 층 0에 스폰 (거주지 위치)
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok());
    EntityId agentId = result.value;

    // 초기 상태
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::Idle);

    // 9시 → WaitingElevator (직장이 다른 층)
    f.agents.update(f.reg, GameTime{0, 9, 0});

    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);
}

// ── Test 2: WaitingElevator NPC → Boarding 엘리베이터 탑승 ─────────────────────

TEST_CASE("AgentElevator - WaitingElevator NPC boards elevator in Boarding state", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId agentId = result.value;

    // 9시 → WaitingElevator
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // 엘리베이터를 층 0 Boarding 상태로 만들기
    // callElevator(0, 0, Up) 이미 호출됨 → 틱만 진행
    // tick1: Idle → (floor 0에 이미 있음) → DoorOpening
    f.transport.update(GameTime{0, 9, 0});
    // tick2: DoorOpening → Boarding
    f.transport.update(GameTime{0, 9, 0});

    auto snap = f.transport.getElevatorState(f.elevId);
    REQUIRE(snap.has_value());
    REQUIRE(snap->currentFloor == 0);
    REQUIRE(snap->state == ElevatorState::Boarding);

    // AgentSystem update → 탑승
    f.agents.update(f.reg, GameTime{0, 9, 0});

    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);

    // 엘리베이터에 탑승자 확인
    snap = f.transport.getElevatorState(f.elevId);
    REQUIRE(snap->passengerCount == 1);
}

// ── Test 3: NPC exits elevator at target floor, PositionComponent updated ──────

TEST_CASE("AgentElevator - NPC exits elevator at target floor, position updated", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId agentId = result.value;

    // 9시 → WaitingElevator
    f.agents.update(f.reg, GameTime{0, 9, 0});

    // 엘리베이터 층 0에서 Boarding 상태로
    f.transport.update(GameTime{0, 9, 0}); // Idle → DoorOpening (이미 0층)
    f.transport.update(GameTime{0, 9, 0}); // DoorOpening → Boarding

    // 탑승
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);

    // 엘리베이터를 2층 DoorOpening/Boarding 상태까지 구동
    // doorTicks=3이므로: Boarding(3틱) → DoorClosing(1틱) → MovingUp(2틱) → DoorOpening = 최소 7틱
    bool atTarget = false;
    for (int i = 0; i < 15 && !atTarget; ++i) {
        f.transport.update(GameTime{0, 9, 0});
        auto snap = f.transport.getElevatorState(f.elevId);
        if (snap && snap->currentFloor == 6 &&
            (snap->state == ElevatorState::DoorOpening ||
             snap->state == ElevatorState::Boarding)) {
            atTarget = true;
        }
    }
    REQUIRE(atTarget);

    // AgentSystem update → 하차
    f.agents.update(f.reg, GameTime{0, 9, 0});

    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::Working);
    const auto& pos = f.reg.get<PositionComponent>(agentId);
    REQUIRE(pos.tile.floor == 6);
}

// ── Test 4: transport_ = nullptr → WaitingElevator 진입 안 함 ─────────────────

TEST_CASE("AgentElevator - no ITransportSystem → does NOT enter WaitingElevator", "[AgentElevator]") {
    // transport 없는 AgentSystem 사용
    EventBus        bus;
    PreloadedConfig cfg;
    GridSystem      grid(bus, cfg);
    AgentSystem     agents(grid, bus);   // transport 없음
    entt::registry  reg;

    grid.buildFloor(1);
    grid.buildFloor(2);

    EntityId home = reg.create();
    grid.placeTenant({2, 0}, TenantType::Residential, 2, home);
    EntityId work = reg.create();
    grid.placeTenant({2, 2}, TenantType::Office, 2, work);

    auto result = agents.spawnAgent(reg, home, work);
    REQUIRE(result.ok());
    EntityId agentId = result.value;

    // 9시 업데이트 — transport 없어도 계단으로 이동 시작
    agents.update(reg, GameTime{0, 9, 0});

    // transport 없으면 계단 사용 (UsingStairs), 엘리베이터 금지
    REQUIRE(agents.getState(reg, agentId) == AgentState::UsingStairs);
    REQUIRE(agents.getState(reg, agentId) != AgentState::WaitingElevator);
}

// ── Test 5: ElevatorPassengerComponent attached/removed ───────────────────────

TEST_CASE("AgentElevator - ElevatorPassengerComponent attached when WaitingElevator, removed after exit", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId agentId = result.value;

    // 초기: ElevatorPassengerComponent 없음
    REQUIRE_FALSE(f.reg.all_of<ElevatorPassengerComponent>(agentId));

    // 9시 → WaitingElevator
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // ElevatorPassengerComponent 부착 확인
    REQUIRE(f.reg.all_of<ElevatorPassengerComponent>(agentId));
    auto& pc = f.reg.get<ElevatorPassengerComponent>(agentId);
    REQUIRE(pc.targetFloor == 6);
    REQUIRE(pc.waiting == true);

    // 엘리베이터 Boarding 상태로
    f.transport.update(GameTime{0, 9, 0});
    f.transport.update(GameTime{0, 9, 0});
    f.agents.update(f.reg, GameTime{0, 9, 0});   // 탑승
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);
    REQUIRE(f.reg.get<ElevatorPassengerComponent>(agentId).waiting == false);

    // 엘리베이터를 2층 DoorOpening/Boarding 상태까지 구동
    bool atTarget = false;
    for (int i = 0; i < 15 && !atTarget; ++i) {
        f.transport.update(GameTime{0, 9, 0});
        auto snap = f.transport.getElevatorState(f.elevId);
        if (snap && snap->currentFloor == 6 &&
            (snap->state == ElevatorState::DoorOpening ||
             snap->state == ElevatorState::Boarding)) {
            atTarget = true;
        }
    }
    REQUIRE(atTarget);

    f.agents.update(f.reg, GameTime{0, 9, 0});   // 하차

    // ElevatorPassengerComponent 제거 확인
    REQUIRE_FALSE(f.reg.all_of<ElevatorPassengerComponent>(agentId));
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::Working);
}

// ── Test 6: NPC does not double-board ─────────────────────────────────────────

TEST_CASE("AgentElevator - NPC does not double-board while InElevator", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId agentId = result.value;

    f.agents.update(f.reg, GameTime{0, 9, 0}); // WaitingElevator

    f.transport.update(GameTime{0, 9, 0}); // DoorOpening
    f.transport.update(GameTime{0, 9, 0}); // Boarding

    f.agents.update(f.reg, GameTime{0, 9, 0}); // 탑승 → InElevator
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);

    int passengersBefore = f.transport.getElevatorState(f.elevId)->passengerCount;
    REQUIRE(passengersBefore == 1);

    // 다시 update — InElevator 상태이므로 탑승 재시도 안 함
    f.agents.update(f.reg, GameTime{0, 9, 0});

    // 여전히 1명 (double-board 없음)
    auto snap = f.transport.getElevatorState(f.elevId);
    REQUIRE(snap->passengerCount == 1);
}

// ── Test 7: callElevator called with correct floor and direction ───────────────

TEST_CASE("AgentElevator - callElevator called with correct floor when WaitingElevator", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId agentId = result.value;

    // 9시 → WaitingElevator (층 0 → 층 2, Up 방향)
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // callElevator가 호출되었으므로 엘리베이터가 0층에서 이동 시작해야 함
    // (elevator는 이미 0층에 있고 callElevator(0, 0, Up)이 호출됨)
    f.transport.update(GameTime{0, 9, 0}); // 0층에서 호출 → 이미 0층이면 DoorOpening

    auto snap = f.transport.getElevatorState(f.elevId);
    REQUIRE(snap.has_value());
    // 0층에 이미 있으므로 DoorOpening 상태가 됨
    REQUIRE(snap->currentFloor == 0);
    REQUIRE(snap->state == ElevatorState::DoorOpening);
}

// ── Test 8: NPC work end while on elevator → cleanup to Idle ─────────────────

TEST_CASE("AgentElevator - NPC re-routes to home when workEndHour reached during elevator", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId agentId = result.value;

    // 9시 → WaitingElevator
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // 탑승
    f.transport.update(GameTime{0, 9, 0});
    f.transport.update(GameTime{0, 9, 0});
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);

    // 엘리베이터를 이동시켜 중간 층(1)까지 이동
    // Boarding 상태에서 doorTicks 소진 → DoorClosing → Moving
    for (int i = 0; i < 5; ++i) {
        f.transport.update(GameTime{0, 9, 0});
    }
    // 엘리베이터가 1층 이상으로 이동했는지 확인 (0층이 아닌 곳)
    auto snap = f.transport.getElevatorState(f.elevId);
    REQUIRE(snap.has_value());
    int elevFloor = snap->currentFloor;

    // 18시 (퇴근 시간) → 귀가 목적지로 재설정
    f.agents.update(f.reg, GameTime{0, 18, 0});

    if (elevFloor == 0) {
        // 엘리베이터가 우연히 홈 층에 있으면 즉시 하차 → Idle
        REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::Idle);
    } else {
        // 엘리베이터가 다른 층에 있으면 재설정 후 계속 탑승
        REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);
        REQUIRE(f.reg.all_of<ElevatorPassengerComponent>(agentId));
        REQUIRE(f.reg.get<ElevatorPassengerComponent>(agentId).targetFloor == 0);
    }
}

// ── Test 9: Two NPCs on same floor both board the same elevator ───────────────

TEST_CASE("AgentElevator - Two NPCs on floor 0 both board same elevator", "[AgentElevator]") {
    ElevFixture f;

    // 2번째 거주지 테넌트 (층 0)
    EntityId home2 = f.reg.create();
    f.grid.placeTenant({6, 0}, TenantType::Residential, 2, home2);

    auto r1 = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    auto r2 = f.agents.spawnAgent(f.reg, home2, f.workTenantId);
    REQUIRE(r1.ok());
    REQUIRE(r2.ok());

    // 9시 → 두 NPC 모두 WaitingElevator
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, r1.value) == AgentState::WaitingElevator);
    REQUIRE(f.agents.getState(f.reg, r2.value) == AgentState::WaitingElevator);

    // 엘리베이터 Boarding 상태로
    f.transport.update(GameTime{0, 9, 0}); // DoorOpening
    f.transport.update(GameTime{0, 9, 0}); // Boarding

    // AgentSystem update → 두 NPC 모두 탑승
    f.agents.update(f.reg, GameTime{0, 9, 0});

    auto snap = f.transport.getElevatorState(f.elevId);
    REQUIRE(snap.has_value());
    // 두 명 모두 탑승 확인 (doorOpenTicks=3이므로 Boarding 상태 유지)
    REQUIRE(snap->passengerCount == 2);
}

// ── Test 10: NPC on same floor as workplace → no WaitingElevator ──────────────

TEST_CASE("AgentElevator - NPC same floor as workplace → normal Working state", "[AgentElevator]") {
    ElevFixture f;

    // 직장을 층 0에 배치 (거주지와 같은 층)
    EntityId sameFloorWork = f.reg.create();
    f.grid.placeTenant({8, 0}, TenantType::Office, 2, sameFloorWork);

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, sameFloorWork);
    REQUIRE(result.ok());
    EntityId agentId = result.value;

    // 9시 → 같은 층이므로 WaitingElevator 안 됨, 바로 Working
    f.agents.update(f.reg, GameTime{0, 9, 0});

    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::Working);
    REQUIRE(f.agents.getState(f.reg, agentId) != AgentState::WaitingElevator);

    // ElevatorPassengerComponent 없음
    REQUIRE_FALSE(f.reg.all_of<ElevatorPassengerComponent>(agentId));
}

// ── Test 11: Return-home elevator — NPC at floor 6 at workEndHour → WaitingElevator ──

TEST_CASE("AgentElevator - NPC at floor 6 at workEndHour transitions to WaitingElevator when home is floor 0", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok());
    EntityId agentId = result.value;

    // 9시 → WaitingElevator (출근: floor 0 → floor 6)
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // 엘리베이터 탑승 후 2층 도착까지 구동
    f.transport.update(GameTime{0, 9, 0}); // DoorOpening
    f.transport.update(GameTime{0, 9, 0}); // Boarding
    f.agents.update(f.reg, GameTime{0, 9, 0}); // 탑승
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);

    // 2층 도착까지 진행
    bool atTarget = false;
    for (int i = 0; i < 15 && !atTarget; ++i) {
        f.transport.update(GameTime{0, 9, 0});
        auto snap = f.transport.getElevatorState(f.elevId);
        if (snap && snap->currentFloor == 6 &&
            (snap->state == ElevatorState::DoorOpening ||
             snap->state == ElevatorState::Boarding)) {
            atTarget = true;
        }
    }
    REQUIRE(atTarget);

    // 하차 → Working at floor 6
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::Working);
    REQUIRE(f.reg.get<PositionComponent>(agentId).tile.floor == 6);

    // 18시 (workEndHour) → WaitingElevator (귀가: floor 6 → floor 0)
    f.agents.update(f.reg, GameTime{0, 18, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // ElevatorPassengerComponent 확인: targetFloor == 0 (home)
    REQUIRE(f.reg.all_of<ElevatorPassengerComponent>(agentId));
    auto& pc = f.reg.get<ElevatorPassengerComponent>(agentId);
    REQUIRE(pc.targetFloor == 0);
    REQUIRE(pc.waiting == true);
}

// ── Test 12: Phantom agent fix — NPC re-routes to home floor when workEndHour while InElevator ──

TEST_CASE("AgentElevator - NPC re-routes to home floor when workEndHour reached while InElevator", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok());
    EntityId agentId = result.value;

    // 9시 → WaitingElevator (출근: floor 0 → floor 6)
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // 엘리베이터 탑승
    f.transport.update(GameTime{0, 9, 0}); // DoorOpening
    f.transport.update(GameTime{0, 9, 0}); // Boarding
    f.agents.update(f.reg, GameTime{0, 9, 0}); // 탑승
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);

    // 탑승 중 targetFloor == 6 (직장)
    REQUIRE(f.reg.get<ElevatorPassengerComponent>(agentId).targetFloor == 6);

    // 엘리베이터를 이동시켜 1층 이상으로 (0층이 아닌 곳)
    // Boarding(doorTicks=3): 3틱 소진 → DoorClosing: 1틱 → MovingUp: 1틱 → floor 1
    for (int i = 0; i < 5; ++i) {
        f.transport.update(GameTime{0, 9, 0});
    }
    auto snap = f.transport.getElevatorState(f.elevId);
    REQUIRE(snap.has_value());
    // 엘리베이터가 0층이 아닌 곳에 있어야 재설정 테스트가 의미 있음
    REQUIRE(snap->currentFloor > 0);

    // 18시 (workEndHour) 도달 — 탑승 중 퇴근
    f.agents.update(f.reg, GameTime{0, 18, 0});

    // NPC는 아직 InElevator — targetFloor가 0(home)으로 변경됨
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);
    REQUIRE(f.reg.all_of<ElevatorPassengerComponent>(agentId));
    REQUIRE(f.reg.get<ElevatorPassengerComponent>(agentId).targetFloor == 0);
}

// ── Test 13: Return-home full trip — NPC arrives at home floor and transitions to Idle ──

TEST_CASE("AgentElevator - NPC arrives at home floor via elevator and transitions to Idle", "[AgentElevator]") {
    ElevFixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok());
    EntityId agentId = result.value;

    // 출근: 9시 → 엘리베이터 → 2층 Working
    f.agents.update(f.reg, GameTime{0, 9, 0});
    f.transport.update(GameTime{0, 9, 0}); // DoorOpening
    f.transport.update(GameTime{0, 9, 0}); // Boarding
    f.agents.update(f.reg, GameTime{0, 9, 0}); // 탑승

    bool atWork = false;
    for (int i = 0; i < 15 && !atWork; ++i) {
        f.transport.update(GameTime{0, 9, 0});
        auto snap = f.transport.getElevatorState(f.elevId);
        if (snap && snap->currentFloor == 6 &&
            (snap->state == ElevatorState::DoorOpening ||
             snap->state == ElevatorState::Boarding)) {
            atWork = true;
        }
    }
    REQUIRE(atWork);
    f.agents.update(f.reg, GameTime{0, 9, 0}); // 하차 → Working
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::Working);
    REQUIRE(f.reg.get<PositionComponent>(agentId).tile.floor == 6);

    // 퇴근: 18시 → WaitingElevator (floor 6 → floor 0)
    f.agents.update(f.reg, GameTime{0, 18, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::WaitingElevator);

    // 엘리베이터를 2층 Boarding 상태로 (callElevator(0, 2, Down) 이미 호출됨)
    // 엘리베이터는 현재 2층에 있을 수 있으므로 tick 진행
    bool elevAtFloor2Boarding = false;
    for (int i = 0; i < 15 && !elevAtFloor2Boarding; ++i) {
        f.transport.update(GameTime{0, 18, 0});
        auto snap = f.transport.getElevatorState(f.elevId);
        if (snap && snap->currentFloor == 6 &&
            (snap->state == ElevatorState::DoorOpening ||
             snap->state == ElevatorState::Boarding)) {
            elevAtFloor2Boarding = true;
        }
    }
    REQUIRE(elevAtFloor2Boarding);

    // 탑승
    f.agents.update(f.reg, GameTime{0, 18, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::InElevator);

    // 0층 도착까지 진행
    bool atHome = false;
    for (int i = 0; i < 15 && !atHome; ++i) {
        f.transport.update(GameTime{0, 18, 0});
        auto snap = f.transport.getElevatorState(f.elevId);
        if (snap && snap->currentFloor == 0 &&
            (snap->state == ElevatorState::DoorOpening ||
             snap->state == ElevatorState::Boarding)) {
            atHome = true;
        }
    }
    REQUIRE(atHome);

    // 하차 → Idle at floor 0
    f.agents.update(f.reg, GameTime{0, 18, 0});
    REQUIRE(f.agents.getState(f.reg, agentId) == AgentState::Idle);
    REQUIRE(f.reg.get<PositionComponent>(agentId).tile.floor == 0);
    REQUIRE_FALSE(f.reg.all_of<ElevatorPassengerComponent>(agentId));
}
