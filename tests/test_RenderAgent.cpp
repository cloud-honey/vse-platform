#include "core/IRenderCommand.h"
#include "core/IAgentSystem.h"
#include "renderer/RenderFrameCollector.h"
#include "domain/GridSystem.h"
#include "domain/TransportSystem.h"
#include "domain/AgentSystem.h"
#include "core/ConfigManager.h"
#include "core/EventBus.h"
#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

using namespace vse;

// ── 테스트 픽스처 ─────────────────────────────────────────

struct PreloadedCfg : ConfigManager {
    PreloadedCfg() {
        loadFromFile(std::string(VSE_PROJECT_ROOT) + "/assets/config/game_config.json");
    }
};

struct RenderAgentFixture {
    EventBus         bus;
    PreloadedCfg     cfg;
    GridSystem       grid;
    TransportSystem  transport;
    AgentSystem      agentSys;
    entt::registry   registry;

    EntityId homeId1  = INVALID_ENTITY;
    EntityId homeId2  = INVALID_ENTITY;
    EntityId homeId3  = INVALID_ENTITY;
    EntityId workId   = INVALID_ENTITY;

    RenderAgentFixture()
        : grid(bus, cfg)
        , transport(grid, bus, cfg)
        , agentSys(grid, bus)
    {

        grid.buildFloor(1);

        homeId1 = registry.create();
        homeId2 = registry.create();
        homeId3 = registry.create();
        workId  = registry.create();

        grid.placeTenant({0, 0},  TenantType::Residential, 1, homeId1);
        grid.placeTenant({5, 0},  TenantType::Residential, 1, homeId2);
        grid.placeTenant({10, 0}, TenantType::Residential, 1, homeId3);
        grid.placeTenant({0, 1},  TenantType::Office,      2, workId);
    }
};

// ── RenderAgentCmd 구조체 테스트 ────────────────────────────

TEST_CASE("RenderAgentCmd - 기본 생성", "[RenderAgent]") {
    RenderAgentCmd ra;
    REQUIRE(ra.id == INVALID_ENTITY);
    REQUIRE(ra.state == AgentState::Idle);
    REQUIRE(ra.facing == Direction::Right);
    REQUIRE(ra.spriteFrame == 0);  // Phase 1 고정
}

TEST_CASE("RenderAgentCmd - spriteFrame Phase1 기본값 0", "[RenderAgent]") {
    RenderAgentCmd ra;
    ra.state = AgentState::Working;
    REQUIRE(ra.spriteFrame == 0);  // 명시적 설정 없이도 0
}

TEST_CASE("RenderAgentCmd - 상태별 값 설정", "[RenderAgent]") {
    RenderAgentCmd ra;
    ra.state  = AgentState::Working;
    ra.facing = Direction::Left;
    ra.pos  = {100, 200};
    REQUIRE(ra.state == AgentState::Working);
    REQUIRE(ra.facing == Direction::Left);
    REQUIRE(ra.pos.x == 100);
    REQUIRE(ra.pos.y == 200);
}

TEST_CASE("RenderFrame - agents 필드 초기 빈 배열", "[RenderAgent]") {
    RenderFrame frame;
    REQUIRE(frame.agents.empty());
}

TEST_CASE("RenderFrame - agents 추가", "[RenderAgent]") {
    RenderFrame frame;

    RenderAgentCmd a1;
    a1.state = AgentState::Working;
    a1.pos = {32, 64};
    frame.agents.push_back(a1);

    RenderAgentCmd a2;
    a2.state = AgentState::Resting;
    a2.pos = {64, 96};
    frame.agents.push_back(a2);

    REQUIRE(frame.agents.size() == 2);
    REQUIRE(frame.agents[0].state == AgentState::Working);
    REQUIRE(frame.agents[1].state == AgentState::Resting);
    REQUIRE(frame.agents[0].pos.x == 32);
    REQUIRE(frame.agents[1].pos.y == 96);
}

TEST_CASE("RenderAgentCmd - facing 방향 저장", "[RenderAgent]") {
    RenderFrame frame;
    RenderAgentCmd ra;
    ra.facing = Direction::Left;
    frame.agents.push_back(ra);
    REQUIRE(frame.agents[0].facing == Direction::Left);
}

// ── RenderFrameCollector + AgentSystem 연동 ─────────────

TEST_CASE("RenderFrameCollector - AgentSystem 미연결 시 agents 빈 배열", "[RenderAgent]") {
    RenderAgentFixture f;
    RenderFrameCollector collector(f.grid, f.transport, 32);
    // setAgentSource 미호출

    auto frame = collector.collect();
    REQUIRE(frame.agents.empty());
}

TEST_CASE("RenderFrameCollector - AgentSystem 연결 후 에이전트 1명 수집", "[RenderAgent]") {
    RenderAgentFixture f;

    auto agentRes = f.agentSys.spawnAgent(f.registry, f.homeId1, f.workId);
    REQUIRE(agentRes.ok());

    RenderFrameCollector collector(f.grid, f.transport, 32);
    collector.setAgentSource(&f.agentSys, &f.registry);

    auto frame = collector.collect();
    REQUIRE(frame.agents.size() == 1);
    REQUIRE(frame.agents[0].state == AgentState::Idle);
}

TEST_CASE("RenderFrameCollector - 에이전트 픽셀 위치 수집 (음수 아님)", "[RenderAgent]") {
    RenderAgentFixture f;

    f.agentSys.spawnAgent(f.registry, f.homeId1, INVALID_ENTITY,
                          TileCoord{0, 0});

    RenderFrameCollector collector(f.grid, f.transport, 32);
    collector.setAgentSource(&f.agentSys, &f.registry);

    auto frame = collector.collect();
    REQUIRE(frame.agents.size() == 1);
    REQUIRE(frame.agents[0].pos.x >= 0);
    REQUIRE(frame.agents[0].pos.y >= 0);
}

TEST_CASE("RenderFrameCollector - 여러 에이전트 수집", "[RenderAgent]") {
    RenderAgentFixture f;

    f.agentSys.spawnAgent(f.registry, f.homeId1, INVALID_ENTITY);
    f.agentSys.spawnAgent(f.registry, f.homeId2, INVALID_ENTITY);
    f.agentSys.spawnAgent(f.registry, f.homeId3, INVALID_ENTITY);

    RenderFrameCollector collector(f.grid, f.transport, 32);
    collector.setAgentSource(&f.agentSys, &f.registry);

    auto frame = collector.collect();
    REQUIRE(frame.agents.size() == 3);
}

TEST_CASE("RenderFrameCollector - despawn 후 수집 안됨", "[RenderAgent]") {
    RenderAgentFixture f;

    auto agentId = f.agentSys.spawnAgent(f.registry, f.homeId1, INVALID_ENTITY).value;
    f.agentSys.despawnAgent(f.registry, agentId);

    RenderFrameCollector collector(f.grid, f.transport, 32);
    collector.setAgentSource(&f.agentSys, &f.registry);

    auto frame = collector.collect();
    REQUIRE(frame.agents.empty());
}

TEST_CASE("RenderFrameCollector - agents Y-sort 오름차순", "[RenderAgent]") {
    // Y-sort: pos.y 오름차순 → 낮은 층(y 작음) 먼저 그려져서 높은 층 NPC가 앞에 보임
    RenderAgentFixture f;

    f.agentSys.spawnAgent(f.registry, f.homeId3, INVALID_ENTITY, TileCoord{10, 0});
    f.agentSys.spawnAgent(f.registry, f.homeId1, INVALID_ENTITY, TileCoord{0, 0});
    f.agentSys.spawnAgent(f.registry, f.homeId2, INVALID_ENTITY, TileCoord{5, 0});

    RenderFrameCollector collector(f.grid, f.transport, 32);
    collector.setAgentSource(&f.agentSys, &f.registry);

    auto frame = collector.collect();
    REQUIRE(frame.agents.size() == 3);

    // Y-sort 검증: pos.y 오름차순
    for (size_t i = 1; i < frame.agents.size(); ++i) {
        REQUIRE(frame.agents[i - 1].pos.y <= frame.agents[i].pos.y);
    }
}

TEST_CASE("RenderFrameCollector - 에이전트 상태 변경 반영", "[RenderAgent]") {
    RenderAgentFixture f;

    f.agentSys.spawnAgent(f.registry, f.homeId1, f.workId);

    // 출근 시간으로 update → Working 상태 전환
    GameTime workTime;
    workTime.day  = 1;
    workTime.hour = 9;
    workTime.minute = 0;
    f.agentSys.update(f.registry, workTime);

    RenderFrameCollector collector(f.grid, f.transport, 32);
    collector.setAgentSource(&f.agentSys, &f.registry);

    auto frame = collector.collect();
    REQUIRE(frame.agents.size() == 1);
    REQUIRE(frame.agents[0].state == AgentState::Working);
}
