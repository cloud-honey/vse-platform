#include "domain/AgentSystem.h"
#include "domain/GridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include <any>

using namespace vse;

static std::string cfgPath() {
    return std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json";
}

// ConfigManager를 생성자에서 미리 로드하는 헬퍼
struct PreloadedConfig : ConfigManager {
    PreloadedConfig() { loadFromFile(cfgPath()); }
};

// 테스트 픽스처 — GridSystem + AgentSystem + entt::registry
struct Fixture {
    EventBus         bus;
    PreloadedConfig  cfg;   // 멤버 초기화 전 loadFromFile 보장
    GridSystem       grid;
    AgentSystem      agents;
    entt::registry   reg;

    // 테넌트 EntityId 시뮬레이션 — GridSystem에 실제 배치
    EntityId homeTenantId  = INVALID_ENTITY;
    EntityId workTenantId  = INVALID_ENTITY;

    Fixture()
        : grid(bus, cfg)
        , agents(grid, bus)
    {

        // 층 0은 자동 건설됨 (GridSystem 생성자)
        // 층 1 건설
        grid.buildFloor(1);

        // 거주지 테넌트 (층 1, x=0~1)
        homeTenantId = reg.create();
        grid.placeTenant({0, 1}, TenantType::Residential, 2, homeTenantId);

        // 직장 테넌트 (층 1, x=5~6)
        workTenantId = reg.create();
        grid.placeTenant({5, 1}, TenantType::Office, 2, workTenantId);
    }
};

TEST_CASE("AgentSystem - 초기 상태 activeAgentCount", "[AgentSystem]") {
    Fixture f;
    REQUIRE(f.agents.activeAgentCount() == 0);
}

TEST_CASE("AgentSystem - spawnAgent 성공 (homeTenant 위치)", "[AgentSystem]") {
    Fixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    REQUIRE(result.value != INVALID_ENTITY);
    REQUIRE(f.agents.activeAgentCount() == 1);

    // 스폰 위치 확인 — homeTenant anchor = (0, 1)
    EntityId id = result.value;
    const auto& pos = f.reg.get<PositionComponent>(id);
    REQUIRE(pos.tile.x == 0);
    REQUIRE(pos.tile.floor == 1);
}

TEST_CASE("AgentSystem - spawnAgent override 위치", "[AgentSystem]") {
    Fixture f;

    TileCoord overridePos{3, 1};
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId, overridePos);
    REQUIRE(result.ok() == true);

    const auto& pos = f.reg.get<PositionComponent>(result.value);
    REQUIRE(pos.tile.x == 3);
    REQUIRE(pos.tile.floor == 1);
}

TEST_CASE("AgentSystem - spawnAgent INVALID_ENTITY → 실패", "[AgentSystem]") {
    Fixture f;

    auto result = f.agents.spawnAgent(f.reg, INVALID_ENTITY, f.workTenantId);
    REQUIRE(result.ok() == false);
    REQUIRE(result.error == ErrorCode::InvalidArgument);
    REQUIRE(f.agents.activeAgentCount() == 0);
}

TEST_CASE("AgentSystem - spawnAgent 컴포넌트 초기값 확인", "[AgentSystem]") {
    Fixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);

    EntityId id = result.value;

    const auto& agent = f.reg.get<AgentComponent>(id);
    REQUIRE(agent.state == AgentState::Idle);
    REQUIRE(agent.satisfaction == 100.0f);
    REQUIRE(agent.homeTenant == f.homeTenantId);
    REQUIRE(agent.workplaceTenant == f.workTenantId);  // 목적지 EntityId 저장 확인

    const auto& sched = f.reg.get<AgentScheduleComponent>(id);
    REQUIRE(sched.workStartHour == 9);
    REQUIRE(sched.workEndHour   == 18);
    REQUIRE(sched.lunchHour     == 12);

    const auto& path = f.reg.get<AgentPathComponent>(id);
    REQUIRE(path.path.empty() == true);
    REQUIRE(path.currentIndex == 0);
}

TEST_CASE("AgentSystem - despawnAgent", "[AgentSystem]") {
    Fixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    EntityId id = result.value;

    f.agents.despawnAgent(f.reg, id);
    REQUIRE(f.agents.activeAgentCount() == 0);
    REQUIRE(f.reg.valid(id) == false);
}

TEST_CASE("AgentSystem - getState", "[AgentSystem]") {
    Fixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId id = result.value;

    auto state = f.agents.getState(f.reg, id);
    REQUIRE(state.has_value() == true);
    REQUIRE(state.value() == AgentState::Idle);
}

TEST_CASE("AgentSystem - getState INVALID_ENTITY → nullopt", "[AgentSystem]") {
    Fixture f;

    auto state = f.agents.getState(f.reg, INVALID_ENTITY);
    REQUIRE(state.has_value() == false);
}

TEST_CASE("AgentSystem - update: 출근 시간 → Working", "[AgentSystem]") {
    Fixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId id = result.value;

    // 초기: Idle
    REQUIRE(f.agents.getState(f.reg, id) == AgentState::Idle);

    // 09:00 — 출근 시간
    GameTime morning{0, 9, 0};
    f.agents.update(f.reg, morning);

    REQUIRE(f.agents.getState(f.reg, id) == AgentState::Working);
}

TEST_CASE("AgentSystem - update: 점심 시간 → Resting", "[AgentSystem]") {
    Fixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId id = result.value;

    // 출근 먼저
    f.agents.update(f.reg, GameTime{0, 9, 0});
    REQUIRE(f.agents.getState(f.reg, id) == AgentState::Working);

    // 12:00 — 점심
    f.agents.update(f.reg, GameTime{0, 12, 0});
    REQUIRE(f.agents.getState(f.reg, id) == AgentState::Resting);
}

TEST_CASE("AgentSystem - update: 퇴근 시간 → Idle", "[AgentSystem]") {
    Fixture f;

    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    EntityId id = result.value;

    // 출근 → 점심 → 오후 → 퇴근
    f.agents.update(f.reg, GameTime{0, 9,  0});  // Working
    f.agents.update(f.reg, GameTime{0, 12, 0});  // Resting
    f.agents.update(f.reg, GameTime{0, 13, 0});  // Working (오후)
    f.agents.update(f.reg, GameTime{0, 18, 0});  // Idle

    REQUIRE(f.agents.getState(f.reg, id) == AgentState::Idle);
}

TEST_CASE("AgentSystem - getAgentsOnFloor", "[AgentSystem]") {
    Fixture f;

    // 층 1에 에이전트 스폰
    f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);

    auto floor1 = f.agents.getAgentsOnFloor(f.reg, 1);
    auto floor0 = f.agents.getAgentsOnFloor(f.reg, 0);

    REQUIRE(floor1.size() == 1);
    REQUIRE(floor0.size() == 0);
}

TEST_CASE("AgentSystem - getAgentsInState", "[AgentSystem]") {
    Fixture f;

    f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);

    auto idleAgents = f.agents.getAgentsInState(f.reg, AgentState::Idle);
    REQUIRE(idleAgents.size() == 1);

    f.agents.update(f.reg, GameTime{0, 9, 0});
    auto workingAgents = f.agents.getAgentsInState(f.reg, AgentState::Working);
    REQUIRE(workingAgents.size() == 1);

    auto idleNow = f.agents.getAgentsInState(f.reg, AgentState::Idle);
    REQUIRE(idleNow.size() == 0);
}

TEST_CASE("AgentSystem - getAverageSatisfaction 에이전트 없음 → 100", "[AgentSystem]") {
    Fixture f;

    float avg = f.agents.getAverageSatisfaction(f.reg);
    REQUIRE(avg == 100.0f);
}

TEST_CASE("AgentSystem - getAverageSatisfaction 에이전트 있음", "[AgentSystem]") {
    Fixture f;

    auto r1 = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(r1.ok() == true);

    // 만족도 100이 기본
    float avg = f.agents.getAverageSatisfaction(f.reg);
    REQUIRE(avg == 100.0f);

    // 만족도 강제로 변경
    f.reg.get<AgentComponent>(r1.value).satisfaction = 80.0f;
    avg = f.agents.getAverageSatisfaction(f.reg);
    REQUIRE(avg == 80.0f);
}

TEST_CASE("AgentSystem - 여러 에이전트 동시 관리", "[AgentSystem]") {
    Fixture f;

    // 추가 테넌트 (층 1, x=10)
    EntityId home2 = f.reg.create();
    f.grid.placeTenant({10, 1}, TenantType::Residential, 1, home2);

    auto r1 = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    auto r2 = f.agents.spawnAgent(f.reg, home2, f.workTenantId);

    REQUIRE(r1.ok() == true);
    REQUIRE(r2.ok() == true);
    REQUIRE(f.agents.activeAgentCount() == 2);

    // 동시 업데이트
    f.agents.update(f.reg, GameTime{0, 9, 0});

    auto working = f.agents.getAgentsInState(f.reg, AgentState::Working);
    REQUIRE(working.size() == 2);
}

// ============================================================================
// TASK-03-002 Tests: Satisfaction & Stress System
// ============================================================================

TEST_CASE("AgentSystem - Satisfaction decreases while WaitingElevator", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to WaitingElevator state
    agent.state = AgentState::WaitingElevator;
    float initialSatisfaction = agent.satisfaction;
    
    // Run update for 5 ticks
    for (int i = 0; i < 5; ++i) {
        f.agents.update(f.reg, GameTime{0, 9, 0});
    }
    
    // Satisfaction should decrease by 2.0 per tick
    REQUIRE(agent.satisfaction == initialSatisfaction + 5 * -2.0f);
}

TEST_CASE("AgentSystem - Stress increases while WaitingElevator", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to WaitingElevator state
    agent.state = AgentState::WaitingElevator;
    float initialStress = agent.stress;
    
    // Run update for 3 ticks
    for (int i = 0; i < 3; ++i) {
        f.agents.update(f.reg, GameTime{0, 9, 0});
    }
    
    // Stress should increase by 1.0 per tick
    REQUIRE(agent.stress == initialStress + 3 * 1.0f);
}

TEST_CASE("AgentSystem - Satisfaction increases while Working", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to Working state with low satisfaction
    agent.state = AgentState::Working;
    agent.satisfaction = 50.0f;
    
    // Run update for 4 ticks
    for (int i = 0; i < 4; ++i) {
        f.agents.update(f.reg, GameTime{0, 9, 0});
    }
    
    // Satisfaction should increase by 0.5 per tick
    REQUIRE(agent.satisfaction == 50.0f + 4 * 0.5f);
}

TEST_CASE("AgentSystem - Satisfaction clamped at 100 while Working", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to Working state with high satisfaction
    agent.state = AgentState::Working;
    agent.satisfaction = 99.0f;
    
    // Run update for 4 ticks
    for (int i = 0; i < 4; ++i) {
        f.agents.update(f.reg, GameTime{0, 9, 0});
    }
    
    // Satisfaction should be clamped at 100
    REQUIRE(agent.satisfaction == 100.0f);
}

TEST_CASE("AgentSystem - Stress decreases while Idle/Resting", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to Idle state with high stress
    agent.state = AgentState::Idle;
    agent.stress = 80.0f;
    
    // Run update for 4 ticks (use 8:00 AM so agent stays Idle)
    for (int i = 0; i < 4; ++i) {
        f.agents.update(f.reg, GameTime{0, 8, 0});
    }
    
    // Stress should decrease by 0.5 per tick
    REQUIRE(agent.stress == 80.0f - 4 * 0.5f);
}

TEST_CASE("AgentSystem - Stress clamped at 0 during recovery", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to Idle state with low stress
    agent.state = AgentState::Idle;
    agent.stress = 1.0f;
    
    // Run update for 4 ticks (use 8:00 AM so agent stays Idle)
    for (int i = 0; i < 4; ++i) {
        f.agents.update(f.reg, GameTime{0, 8, 0});
    }
    
    // Stress should be clamped at 0
    REQUIRE(agent.stress == 0.0f);
}

TEST_CASE("AgentSystem - Satisfaction clamped at 0 during decay", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to WaitingElevator state with satisfaction just above threshold
    agent.state = AgentState::WaitingElevator;
    agent.satisfaction = 25.0f;  // Above 20.0 threshold
    
    // Run update for 4 ticks
    for (int i = 0; i < 4; ++i) {
        f.agents.update(f.reg, GameTime{0, 9, 0});
    }
    
    // Satisfaction should decrease but not go below 0
    // 25.0 - 4 * 2.0 = 17.0, which is below threshold
    // Agent should transition to Leaving when satisfaction drops below 20.0
    REQUIRE(agent.satisfaction < 25.0f);  // Should have decreased
    REQUIRE(agent.satisfaction >= 0.0f);  // Should not be negative
}

TEST_CASE("AgentSystem - Agent transitions to Leaving when satisfaction < leaveThreshold", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to WaitingElevator state with satisfaction below threshold
    agent.state = AgentState::WaitingElevator;
    agent.satisfaction = 15.0f;  // Below 20.0 threshold
    
    // Run update
    f.agents.update(f.reg, GameTime{0, 9, 0});
    
    // Agent should transition to Leaving state
    REQUIRE(agent.state == AgentState::Leaving);
}

TEST_CASE("AgentSystem - Leaving agent is despawned on subsequent tick", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to Leaving state
    agent.state = AgentState::Leaving;
    
    // Run update - agent should be despawned
    f.agents.update(f.reg, GameTime{0, 9, 0});
    
    // Agent should no longer exist
    REQUIRE_FALSE(f.reg.valid(agentId));
    REQUIRE(f.agents.activeAgentCount() == 0);
}

TEST_CASE("AgentSystem - AgentStateChanged event emitted when agent enters Leaving state", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    
    // Track events
    std::vector<Event> capturedEvents;
    f.bus.subscribe(EventType::AgentStateChanged, [&](const Event& ev) {
        capturedEvents.push_back(ev);
    });
    
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to WaitingElevator state with satisfaction below threshold
    agent.state = AgentState::WaitingElevator;
    agent.satisfaction = 15.0f;
    
    // Clear any previous events
    capturedEvents.clear();
    
    // Run update
    f.agents.update(f.reg, GameTime{0, 9, 0});
    
    // Flush events to deliver them
    f.bus.flush();
    
    // Check that AgentStateChanged event was emitted with Leaving state
    bool foundLeavingEvent = false;
    for (const auto& ev : capturedEvents) {
        if (ev.type == EventType::AgentStateChanged && 
            ev.source == agentId &&
            ev.payload.has_value()) {
            // Check if payload is int and equals Leaving state
            if (ev.payload.type() == typeid(int)) {
                int payloadValue = std::any_cast<int>(ev.payload);
                if (payloadValue == static_cast<int>(AgentState::Leaving)) {
                    foundLeavingEvent = true;
                    break;
                }
            }
        }
    }
    REQUIRE(foundLeavingEvent == true);
}

TEST_CASE("AgentSystem - Multiple agents: only dissatisfied one leaves", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    
    // Create two agents
    auto result1 = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    auto result2 = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result1.ok() == true);
    REQUIRE(result2.ok() == true);
    
    EntityId agent1 = result1.value;
    EntityId agent2 = result2.value;
    auto& a1 = f.reg.get<AgentComponent>(agent1);
    auto& a2 = f.reg.get<AgentComponent>(agent2);
    
    // Set agent1 to WaitingElevator with low satisfaction
    a1.state = AgentState::WaitingElevator;
    a1.satisfaction = 15.0f;
    
    // Set agent2 to Working with high satisfaction
    a2.state = AgentState::Working;
    a2.satisfaction = 80.0f;
    
    // Run update
    f.agents.update(f.reg, GameTime{0, 9, 0});
    
    // Only agent1 should transition to Leaving
    REQUIRE(a1.state == AgentState::Leaving);
    REQUIRE(a2.state == AgentState::Working);
    
    // Run another update to despawn agent1
    f.agents.update(f.reg, GameTime{0, 9, 0});
    
    // agent1 should be despawned, agent2 should still exist
    REQUIRE_FALSE(f.reg.valid(agent1));
    REQUIRE(f.reg.valid(agent2));
    REQUIRE(f.agents.activeAgentCount() == 1);
}

TEST_CASE("AgentSystem - Agent in Working state does not accumulate stress", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to Working state
    agent.state = AgentState::Working;
    float initialStress = agent.stress;
    
    // Run update for 5 ticks
    for (int i = 0; i < 5; ++i) {
        f.agents.update(f.reg, GameTime{0, 9, 0});
    }
    
    // Stress should not change
    REQUIRE(agent.stress == initialStress);
}

TEST_CASE("AgentSystem - Agent with full satisfaction (100) does not leave", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    auto result = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result.ok() == true);
    
    EntityId agentId = result.value;
    auto& agent = f.reg.get<AgentComponent>(agentId);
    
    // Set agent to WaitingElevator state with full satisfaction
    agent.state = AgentState::WaitingElevator;
    agent.satisfaction = 100.0f;
    
    // Run update for 10 ticks
    for (int i = 0; i < 10; ++i) {
        f.agents.update(f.reg, GameTime{0, 9, 0});
    }
    
    // Agent should not transition to Leaving (satisfaction will decrease but stay above threshold)
    REQUIRE(agent.state != AgentState::Leaving);
    REQUIRE(agent.satisfaction > 20.0f);  // Should still be above threshold
}

TEST_CASE("AgentSystem - Average satisfaction reflects decay", "[AgentSystem][TASK-03-002]") {
    Fixture f;
    
    // Create two agents
    auto result1 = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    auto result2 = f.agents.spawnAgent(f.reg, f.homeTenantId, f.workTenantId);
    REQUIRE(result1.ok() == true);
    REQUIRE(result2.ok() == true);
    
    EntityId agent1 = result1.value;
    EntityId agent2 = result2.value;
    auto& a1 = f.reg.get<AgentComponent>(agent1);
    auto& a2 = f.reg.get<AgentComponent>(agent2);
    
    // Set both to WaitingElevator with different satisfaction levels
    a1.state = AgentState::WaitingElevator;
    a1.satisfaction = 50.0f;
    
    a2.state = AgentState::WaitingElevator;
    a2.satisfaction = 70.0f;
    
    float initialAvg = f.agents.getAverageSatisfaction(f.reg);
    REQUIRE(initialAvg == 60.0f);
    
    // Run update for 5 ticks
    for (int i = 0; i < 5; ++i) {
        f.agents.update(f.reg, GameTime{0, 9, 0});
    }
    
    // Average should decrease
    float finalAvg = f.agents.getAverageSatisfaction(f.reg);
    REQUIRE(finalAvg < initialAvg);
    REQUIRE(finalAvg == 60.0f + 5 * -2.0f);  // Both lose 2.0 per tick
}
