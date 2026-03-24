#include "domain/AgentSystem.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace vse {

AgentSystem::AgentSystem(IGridSystem& grid, EventBus& bus)
    : grid_(grid)
    , eventBus_(bus)
{}

Result<EntityId> AgentSystem::spawnAgent(entt::registry& reg,
                                          EntityId homeTenantId,
                                          EntityId workplaceId,
                                          std::optional<TileCoord> spawnPos)
{
    // homeTenant 유효성 검사
    if (homeTenantId == INVALID_ENTITY) {
        spdlog::warn("AgentSystem::spawnAgent: invalid homeTenantId");
        return Result<EntityId>::failure(ErrorCode::InvalidArgument);
    }

    // 스폰 위치 결정 — override 없으면 homeTenant anchor 위치
    TileCoord pos{0, 0};
    if (spawnPos.has_value()) {
        pos = spawnPos.value();
        if (!grid_.isValidCoord(pos)) {
            spdlog::warn("AgentSystem::spawnAgent: invalid spawnPos ({}, {})", pos.x, pos.floor);
            return Result<EntityId>::failure(ErrorCode::InvalidArgument);
        }
    } else {
        // homeTenant 위치 찾기 — GridSystem에서 해당 entity를 가진 anchor 탐색
        // Phase 1: 층 0부터 순회하여 homeTenantId anchor 위치 찾기
        bool found = false;
        for (int floor = 0; floor < grid_.maxFloors() && !found; ++floor) {
            if (!grid_.isFloorBuilt(floor)) continue;
            auto tiles = grid_.getFloorTiles(floor);
            for (auto& [coord, tile] : tiles) {
                if (tile.isAnchor && tile.tenantEntity == homeTenantId) {
                    pos = coord;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            spdlog::warn("AgentSystem::spawnAgent: homeTenant {:d} not found in grid",
                         static_cast<uint32_t>(homeTenantId));
            return Result<EntityId>::failure(ErrorCode::InvalidArgument);
        }
    }

    // ECS 엔티티 생성
    EntityId entity = reg.create();

    // 컴포넌트 부착
    auto& agentComp = reg.emplace<AgentComponent>(entity);
    agentComp.state      = AgentState::Idle;
    agentComp.workplace  = TenantType::Office;  // Phase 1: 기본값
    agentComp.homeTenant = homeTenantId;
    agentComp.satisfaction = 100.0f;
    agentComp.moveSpeed    = 1.0f;

    auto& posComp = reg.emplace<PositionComponent>(entity);
    posComp.tile   = pos;
    posComp.pixel  = {pos.x * 32.0f, pos.floor * 32.0f};  // Phase 1: 32px per tile
    posComp.facing = Direction::Right;

    auto& schedComp = reg.emplace<AgentScheduleComponent>(entity);
    schedComp.workStartHour = 9;
    schedComp.workEndHour   = 18;
    schedComp.lunchHour     = 12;

    auto& pathComp = reg.emplace<AgentPathComponent>(entity);
    pathComp.path.clear();
    pathComp.currentIndex = 0;
    pathComp.destination  = pos;

    // 추적 등록
    activeAgents_.insert(static_cast<uint32_t>(entity));

    spdlog::debug("AgentSystem::spawnAgent: entity {:d} spawned at ({}, {})",
                  static_cast<uint32_t>(entity), pos.x, pos.floor);

    // 이벤트 발행
    Event ev;
    ev.type   = EventType::AgentStateChanged;
    ev.source = entity;
    eventBus_.publish(ev);

    return Result<EntityId>::success(entity);
}

void AgentSystem::despawnAgent(entt::registry& reg, EntityId id)
{
    if (id == INVALID_ENTITY || !reg.valid(id)) {
        spdlog::warn("AgentSystem::despawnAgent: invalid entity");
        return;
    }

    activeAgents_.erase(static_cast<uint32_t>(id));
    reg.destroy(id);

    spdlog::debug("AgentSystem::despawnAgent: entity {:d} removed",
                  static_cast<uint32_t>(id));
}

int AgentSystem::activeAgentCount() const
{
    return static_cast<int>(activeAgents_.size());
}

void AgentSystem::update(entt::registry& reg, const GameTime& time)
{
    // AgentComponent + AgentScheduleComponent 보유 엔티티만 순회
    auto view = reg.view<AgentComponent, AgentScheduleComponent, PositionComponent>();
    for (auto entity : view) {
        auto& agent    = view.get<AgentComponent>(entity);
        auto& schedule = view.get<AgentScheduleComponent>(entity);

        processSchedule(reg, entity, agent, schedule, time);
    }
}

void AgentSystem::processSchedule(entt::registry& reg, EntityId id,
                                   AgentComponent& agent,
                                   const AgentScheduleComponent& schedule,
                                   const GameTime& time)
{
    AgentState prevState = agent.state;

    if (time.hour >= schedule.workStartHour && time.hour < schedule.lunchHour) {
        // 출근 시간 → Working
        if (agent.state == AgentState::Idle) {
            agent.state = AgentState::Working;
        }
    } else if (time.hour == schedule.lunchHour) {
        // 점심 시간 → Resting
        if (agent.state == AgentState::Working) {
            agent.state = AgentState::Resting;
        }
    } else if (time.hour > schedule.lunchHour && time.hour < schedule.workEndHour) {
        // 오후 근무 → Working
        if (agent.state == AgentState::Resting) {
            agent.state = AgentState::Working;
        }
    } else if (time.hour >= schedule.workEndHour) {
        // 퇴근 → Idle
        if (agent.state == AgentState::Working || agent.state == AgentState::Resting) {
            agent.state = AgentState::Idle;
        }
    }

    // 상태 변경 이벤트
    if (agent.state != prevState) {
        Event ev;
        ev.type   = EventType::AgentStateChanged;
        ev.source = id;
        ev.payload = static_cast<int>(agent.state);
        eventBus_.publish(ev);

        spdlog::debug("AgentSystem: entity {:d} state {} → {}",
                      static_cast<uint32_t>(id),
                      static_cast<int>(prevState),
                      static_cast<int>(agent.state));
    }
}

std::optional<AgentState> AgentSystem::getState(entt::registry& reg, EntityId id) const
{
    if (id == INVALID_ENTITY || !reg.valid(id)) return std::nullopt;
    const auto* agent = reg.try_get<AgentComponent>(id);
    if (!agent) return std::nullopt;
    return agent->state;
}

std::vector<EntityId> AgentSystem::getAgentsOnFloor(entt::registry& reg, int floor) const
{
    std::vector<EntityId> result;
    auto view = reg.view<PositionComponent, AgentComponent>();
    for (auto entity : view) {
        const auto& pos = view.get<PositionComponent>(entity);
        if (pos.tile.floor == floor) {
            result.push_back(entity);
        }
    }
    return result;
}

std::vector<EntityId> AgentSystem::getAgentsInState(entt::registry& reg, AgentState state) const
{
    std::vector<EntityId> result;
    auto view = reg.view<AgentComponent>();
    for (auto entity : view) {
        const auto& agent = view.get<AgentComponent>(entity);
        if (agent.state == state) {
            result.push_back(entity);
        }
    }
    return result;
}

float AgentSystem::getAverageSatisfaction(entt::registry& reg) const
{
    auto view = reg.view<AgentComponent>();
    float total = 0.0f;
    int   count = 0;
    for (auto entity : view) {
        const auto& agent = view.get<AgentComponent>(entity);
        total += agent.satisfaction;
        ++count;
    }
    if (count == 0) return 100.0f;  // 에이전트 없으면 만점
    return total / static_cast<float>(count);
}

std::optional<TileCoord> AgentSystem::resolveDestination(EntityId tenantEntityId) const
{
    // GridSystem에서 해당 tenantEntity의 anchor 위치 조회
    for (int floor = 0; floor < grid_.maxFloors(); ++floor) {
        if (!grid_.isFloorBuilt(floor)) continue;
        auto tiles = grid_.getFloorTiles(floor);
        for (auto& [coord, tile] : tiles) {
            if (tile.isAnchor && tile.tenantEntity == tenantEntityId) {
                return coord;
            }
        }
    }
    return std::nullopt;
}

} // namespace vse
