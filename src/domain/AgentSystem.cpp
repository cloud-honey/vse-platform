#include "domain/AgentSystem.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

// TODO(TASK-03-009): Replace with ContentRegistry access when available
// Values mirror balance.json npc.* fields
namespace {
    constexpr float SATISFACTION_GAIN_WORK    =  0.5f;   // npc.satisfactionGainWork
    constexpr float SATISFACTION_LOSS_WAIT    = -2.0f;   // npc.satisfactionLossWait
    // SATISFACTION_LOSS_NO_ELEVATOR (-5.0) deferred to Phase 2 elevator frustration
    constexpr float LEAVE_THRESHOLD           = 20.0f;   // npc.leaveThreshold
    constexpr float STRESS_INCREASE_WAIT      =  1.0f;
    constexpr float STRESS_DECREASE_IDLE      =  0.5f;
    // Tenant-specific decay rates (satisfactionDecayRate) deferred to TASK-03-003 (tenant impl)
}

namespace vse {

AgentSystem::AgentSystem(IGridSystem& grid, EventBus& bus)
    : grid_(grid)
    , eventBus_(bus)
    , transport_(nullptr)
{}

AgentSystem::AgentSystem(IGridSystem& grid, EventBus& bus, ITransportSystem& transport)
    : grid_(grid)
    , eventBus_(bus)
    , transport_(&transport)
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
    agentComp.state           = AgentState::Idle;
    agentComp.homeTenant      = homeTenantId;
    agentComp.workplaceTenant = workplaceId;    // 실제 목적지 EntityId 저장
    agentComp.satisfaction    = 100.0f;
    agentComp.moveSpeed       = 1.0f;
    agentComp.stress          = 0.0f;

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

    // 이벤트 발행 — AgentSpawned (생성 사건)
    Event ev;
    ev.type   = EventType::AgentSpawned;
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

    // 이벤트 발행 — AgentDespawned (제거 사건, destroy 전에 발행)
    Event despawnEv;
    despawnEv.type   = EventType::AgentDespawned;
    despawnEv.source = id;
    eventBus_.publish(despawnEv);

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
    // Collect agents to despawn (Leaving state from previous tick)
    std::vector<EntityId> toDespawn;
    
    // AgentComponent + AgentScheduleComponent 보유 엔티티만 순회
    auto view = reg.view<AgentComponent, AgentScheduleComponent, PositionComponent>();
    for (auto entity : view) {
        auto& agent    = view.get<AgentComponent>(entity);
        auto& schedule = view.get<AgentScheduleComponent>(entity);

        // If agent is in Leaving state, mark for despawn
        if (agent.state == AgentState::Leaving) {
            toDespawn.push_back(entity);
            continue;
        }

        // 계단 이동 상태 처리
        if (agent.state == AgentState::UsingStairs) {
            processStairs(reg, entity, agent, schedule, time);
        }
        // 엘리베이터 대기/탑승 상태 처리 (transport_ 있을 때만)
        else if (transport_ && (agent.state == AgentState::WaitingElevator ||
                           agent.state == AgentState::InElevator)) {
            processElevator(reg, entity, agent, schedule, time);
        } else {
            processSchedule(reg, entity, agent, schedule, time);
        }
        
        // Apply satisfaction decay and stress changes
        updateSatisfactionAndStress(reg, entity, agent, schedule);
    }
    
    // Despawn agents marked as Leaving
    for (auto entity : toDespawn) {
        despawnAgent(reg, entity);
    }
}

void AgentSystem::processSchedule(entt::registry& reg, EntityId id,
                                   AgentComponent& agent,
                                   const AgentScheduleComponent& schedule,
                                   const GameTime& time)
{
    AgentState prevState = agent.state;
    AgentState nextState = agent.state;
    EntityId   destTenant = INVALID_ENTITY;

    if (time.hour >= schedule.workStartHour && time.hour < schedule.lunchHour) {
        // 출근 시간 → Working
        if (agent.state == AgentState::Idle) {
            nextState  = AgentState::Working;
            destTenant = agent.workplaceTenant;
        }
    } else if (time.hour == schedule.lunchHour) {
        // 점심 시간 → Resting
        if (agent.state == AgentState::Working) {
            nextState = AgentState::Resting;
        }
    } else if (time.hour > schedule.lunchHour && time.hour < schedule.workEndHour) {
        // 오후 근무 → Working
        if (agent.state == AgentState::Resting) {
            nextState  = AgentState::Working;
            destTenant = agent.workplaceTenant;
        }
    } else if (time.hour >= schedule.workEndHour) {
        // 퇴근 → Idle
        if (agent.state == AgentState::Working || agent.state == AgentState::Resting) {
            nextState  = AgentState::Idle;
            destTenant = agent.homeTenant;
        }
    }

    // 상태 변경이 있고 목적지가 있을 경우 엘리베이터 필요 여부 확인
    if (nextState != agent.state && destTenant != INVALID_ENTITY && transport_ != nullptr) {
        auto dest = resolveDestination(destTenant);
        if (dest.has_value()) {
            const auto& pos = reg.get<PositionComponent>(id);
            int currentFloor = pos.tile.floor;
            int targetFloor  = dest->floor;

            if (targetFloor != currentFloor) {
                // 계단 vs 엘리베이터 결정
                int floorDiff = std::abs(targetFloor - currentFloor);
                
                if (floorDiff <= 4) {
                    // 계단 사용 — 4층 이하 차이
                    agent.state = AgentState::UsingStairs;
                    agent.stairTargetFloor = targetFloor;
                    agent.stairTicksRemaining = floorDiff * 2;  // 2 ticks per floor
                    agent.elevatorWaitTicks = 0;  // Reset elevator wait counter
                    
                    spdlog::debug("AgentSystem: entity {:d} → UsingStairs floor {} → {} ({} ticks)",
                                  static_cast<uint32_t>(id), currentFloor, targetFloor, 
                                  agent.stairTicksRemaining);

                    // 상태 변경 이벤트
                    Event ev;
                    ev.type    = EventType::AgentStateChanged;
                    ev.source  = id;
                    ev.payload = static_cast<int>(AgentState::UsingStairs);
                    eventBus_.publish(ev);
                    return;
                } else {
                    // 엘리베이터 사용 — 5층 이상 차이
                    agent.state = AgentState::WaitingElevator;
                    agent.elevatorWaitTicks = 0;  // Reset elevator wait counter
                    agent.stairTargetFloor = -1;  // Clear stair target
                    agent.stairTicksRemaining = 0;

                    auto& passengerComp = reg.emplace_or_replace<ElevatorPassengerComponent>(id);
                    passengerComp.targetFloor = targetFloor;
                    passengerComp.waiting     = true;

                    // 방향 결정
                    ElevatorDirection dir = (targetFloor > currentFloor)
                                            ? ElevatorDirection::Up
                                            : ElevatorDirection::Down;

                    transport_->callElevator(0, currentFloor, dir);

                    spdlog::debug("AgentSystem: entity {:d} → WaitingElevator floor {} → {}",
                                  static_cast<uint32_t>(id), currentFloor, targetFloor);

                    // 상태 변경 이벤트
                    Event ev;
                    ev.type    = EventType::AgentStateChanged;
                    ev.source  = id;
                    ev.payload = static_cast<int>(AgentState::WaitingElevator);
                    eventBus_.publish(ev);
                    return;
                }
            }
        }
    }

    // 같은 층 이동 또는 엘리베이터 불필요 — 기존 동작
    agent.state = nextState;

    // 상태 변경 이벤트
    if (agent.state != prevState) {
        // 테넌트 점유자 수 업데이트 (Working 상태 변경 시)
        updateTenantOccupantCount(reg, id, prevState, agent.state);

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

void AgentSystem::processElevator(entt::registry& reg, EntityId id,
                                   AgentComponent& agent,
                                   const AgentScheduleComponent& schedule,
                                   const GameTime& time)
{
    // 퇴근 시간 초과 시 처리 — 귀가 중인 경우는 중단하지 않음
    if (time.hour >= schedule.workEndHour) {
        if (reg.all_of<ElevatorPassengerComponent>(id)) {
            auto& passengerComp = reg.get<ElevatorPassengerComponent>(id);

            // 귀가 목적지(홈 층) 조회
            auto homeDest = resolveDestination(agent.homeTenant);
            int homeFloor = homeDest.has_value() ? homeDest->floor : -1;

            if (!passengerComp.waiting) {
                // Issue 2: 탑승 중(InElevator)이고 퇴근 시간 → 귀가 목적지로 재설정
                // targetFloor가 이미 홈 층이면 그냥 계속 진행
                if (passengerComp.targetFloor != homeFloor && homeFloor >= 0) {
                    passengerComp.targetFloor = homeFloor;
                    // 엘리베이터 carCalls에도 home floor 추가 (hall call 재발행으로 LOOK 알고리즘에 반영)
                    ElevatorDirection homeDir = (homeFloor < reg.get<PositionComponent>(id).tile.floor)
                                                ? ElevatorDirection::Down
                                                : ElevatorDirection::Up;
                    transport_->callElevator(0, homeFloor, homeDir);
                    spdlog::debug("AgentSystem: entity {:d} work end while InElevator, re-route to home floor {} + hall call",
                                  static_cast<uint32_t>(id), homeFloor);
                    // 재설정한 틱에서는 즉시 하차 체크 금지 (같은 틱에 exit 방지)
                    return;
                }
                // 이미 귀가 중(targetFloor == homeFloor) → 정상 하차 로직으로 계속 진행
            } else {
                // WaitingElevator 상태 — 귀가 중이면 계속, 출근 중이면 포기
                // 홈 층 대기 중이면 귀가 목적지로 이미 설정된 것 → 계속 진행
                if (passengerComp.targetFloor == homeFloor) {
                    // 귀가 중 → 엘리베이터 계속 대기 (return 하지 않음)
                } else {
                    // 출근 중 대기 → 포기하고 Idle
                    reg.remove<ElevatorPassengerComponent>(id);
                    agent.state = AgentState::Idle;

                    Event ev;
                    ev.type    = EventType::AgentStateChanged;
                    ev.source  = id;
                    ev.payload = static_cast<int>(AgentState::Idle);
                    eventBus_.publish(ev);

                    spdlog::debug("AgentSystem: entity {:d} work end while waiting for work elevator, forced Idle",
                                  static_cast<uint32_t>(id));
                    return;
                }
            }
        } else {
            // ElevatorPassengerComponent 없는데 WaitingElevator/InElevator → Idle로 복구
            agent.state = AgentState::Idle;

            Event ev;
            ev.type    = EventType::AgentStateChanged;
            ev.source  = id;
            ev.payload = static_cast<int>(AgentState::Idle);
            eventBus_.publish(ev);

            spdlog::debug("AgentSystem: entity {:d} work end, no passenger comp, forced Idle",
                          static_cast<uint32_t>(id));
            return;
        }
    }

    if (!reg.all_of<ElevatorPassengerComponent>(id)) {
        // 컴포넌트가 없으면 스케줄로 복귀
        processSchedule(reg, id, agent, schedule, time);
        return;
    }

    auto& passengerComp = reg.get<ElevatorPassengerComponent>(id);
    const auto& pos = reg.get<PositionComponent>(id);

    if (passengerComp.waiting) {
        // 엘리베이터 대기 시간 증가
        agent.elevatorWaitTicks++;
        
        // 대기 시간 초과 체크 (20 ticks = 2 seconds)
        if (agent.elevatorWaitTicks > 20) {
            int currentFloor = pos.tile.floor;
            int targetFloor = passengerComp.targetFloor;
            int floorDiff = std::abs(targetFloor - currentFloor);
            
            // 4층 이하 차이면 계단으로 전환
            if (floorDiff <= 4) {
                spdlog::debug("AgentSystem: entity {:d} elevator wait timeout ({} ticks), switching to stairs floor {} → {}",
                              static_cast<uint32_t>(id), agent.elevatorWaitTicks,
                              currentFloor, targetFloor);
                
                // 엘리베이터 대기 취소
                reg.remove<ElevatorPassengerComponent>(id);
                
                // 계단으로 전환
                agent.state = AgentState::UsingStairs;
                agent.stairTargetFloor = targetFloor;
                agent.stairTicksRemaining = floorDiff * 2;
                agent.elevatorWaitTicks = 0;
                
                Event ev;
                ev.type    = EventType::AgentStateChanged;
                ev.source  = id;
                ev.payload = static_cast<int>(AgentState::UsingStairs);
                eventBus_.publish(ev);
                return;
            }
            // 5층 이상 차이면 계속 대기 (초과하지 않음)
        }
        
        // 대기 중 — 현재 층에 Boarding 상태의 엘리베이터가 있는지 확인
        int currentFloor = pos.tile.floor;
        auto elevatorsAtFloor = transport_->getElevatorsAtFloor(currentFloor);

        for (auto elevId : elevatorsAtFloor) {
            auto snap = transport_->getElevatorState(elevId);
            if (!snap.has_value()) continue;
            if (snap->state == ElevatorState::Boarding &&
                snap->currentFloor == currentFloor) {
                // 탑승 시도
                auto boardResult = transport_->boardPassenger(
                    elevId, id, passengerComp.targetFloor);
                if (boardResult.ok()) {
                    passengerComp.waiting = false;
                    agent.state = AgentState::InElevator;
                    agent.elevatorWaitTicks = 0;  // Reset wait counter on boarding

                    Event ev;
                    ev.type    = EventType::AgentStateChanged;
                    ev.source  = id;
                    ev.payload = static_cast<int>(AgentState::InElevator);
                    eventBus_.publish(ev);

                    spdlog::debug("AgentSystem: entity {:d} boarded elevator {:d}",
                                  static_cast<uint32_t>(id),
                                  static_cast<uint32_t>(elevId));
                }
                break;  // 한 엘리베이터만 시도
            }
        }
    } else {
        // 탑승 중 — 목적 층 도착 여부 확인
        int targetFloor = passengerComp.targetFloor;
        auto allElevators = transport_->getAllElevators();

        for (auto elevId : allElevators) {
            auto snap = transport_->getElevatorState(elevId);
            if (!snap.has_value()) continue;

            // 이 에이전트가 이 엘리베이터에 탑승 중인지 확인
            bool onThisElevator = false;
            for (auto p : snap->passengers) {
                if (p == id) { onThisElevator = true; break; }
            }
            if (!onThisElevator) continue;

            // 목적 층 도착 + 탑승/하차 가능 상태 확인
            if (snap->currentFloor == targetFloor &&
                (snap->state == ElevatorState::DoorOpening ||
                 snap->state == ElevatorState::Boarding)) {
                // 하차
                transport_->exitPassenger(elevId, id);

                // 위치 업데이트
                auto& posComp = reg.get<PositionComponent>(id);
                posComp.tile.floor = targetFloor;
                posComp.pixel.y    = static_cast<float>(targetFloor * 32);

                // 컴포넌트 제거
                reg.remove<ElevatorPassengerComponent>(id);
                agent.elevatorWaitTicks = 0;  // Reset wait counter

                // 상태 전환 — 직장 or 집 도착
                AgentState newState = AgentState::Working;
                // 퇴근 시간 이후이면 Idle
                if (time.hour >= schedule.workEndHour) {
                    newState = AgentState::Idle;
                }
                
                // 테넌트 점유자 수 업데이트 (상태 변경 시)
                updateTenantOccupantCount(reg, id, agent.state, newState);
                agent.state = newState;

                Event ev;
                ev.type    = EventType::AgentStateChanged;
                ev.source  = id;
                ev.payload = static_cast<int>(newState);
                eventBus_.publish(ev);

                spdlog::debug("AgentSystem: entity {:d} exited elevator at floor {}",
                              static_cast<uint32_t>(id), targetFloor);
                return;
            }
        }
    }
}

void AgentSystem::processStairs(entt::registry& reg, EntityId id,
                                 AgentComponent& agent,
                                 const AgentScheduleComponent& schedule,
                                 const GameTime& time)
{
    // 계단 이동 진행
    if (agent.stairTicksRemaining > 0) {
        agent.stairTicksRemaining--;
        
        spdlog::debug("AgentSystem: entity {:d} stair progress {}/{} ticks",
                      static_cast<uint32_t>(id),
                      agent.stairTicksRemaining,
                      agent.stairTargetFloor);
        
        // 만약 0이 되었으면 즉시 완료 처리
        if (agent.stairTicksRemaining == 0) {
            // 계단 이동 완료
            int targetFloor = agent.stairTargetFloor;
            if (targetFloor >= 0) {
                // 위치 업데이트
                auto& posComp = reg.get<PositionComponent>(id);
                posComp.tile.floor = targetFloor;
                posComp.pixel.y    = static_cast<float>(targetFloor * 32);
                
                // 계단 필드 초기화
                agent.stairTargetFloor = -1;
                agent.stairTicksRemaining = 0;
                
                // 상태 전환 — 직장 or 집 도착
                AgentState newState = AgentState::Working;
                // 퇴근 시간 이후이면 Idle
                if (time.hour >= schedule.workEndHour) {
                    newState = AgentState::Idle;
                }
                
                // 테넌트 점유자 수 업데이트 (상태 변경 시)
                updateTenantOccupantCount(reg, id, agent.state, newState);
                AgentState oldState = agent.state;
                agent.state = newState;

                Event ev;
                ev.type    = EventType::AgentStateChanged;
                ev.source  = id;
                ev.payload = static_cast<int>(newState);
                eventBus_.publish(ev);

                spdlog::debug("AgentSystem: entity {:d} finished stairs at floor {} → state {} (was {})",
                              static_cast<uint32_t>(id), targetFloor, static_cast<int>(newState),
                              static_cast<int>(oldState));
            } else {
                // 잘못된 상태 — Idle로 복구
                spdlog::warn("AgentSystem: entity {:d} in UsingStairs but stairTargetFloor = -1, resetting to Idle",
                             static_cast<uint32_t>(id));
                agent.state = AgentState::Idle;
                agent.stairTargetFloor = -1;
                agent.stairTicksRemaining = 0;
                
                Event ev;
                ev.type    = EventType::AgentStateChanged;
                ev.source  = id;
                ev.payload = static_cast<int>(AgentState::Idle);
                eventBus_.publish(ev);
            }
        }
        // 아직 이동 중 (stairTicksRemaining > 0)
        return;
    }
    
    // stairTicksRemaining이 이미 0인 경우 (이상 상태)
    spdlog::warn("AgentSystem: entity {:d} in UsingStairs but stairTicksRemaining = 0, resetting to Idle",
                 static_cast<uint32_t>(id));
    agent.state = AgentState::Idle;
    agent.stairTargetFloor = -1;
    
    Event ev;
    ev.type    = EventType::AgentStateChanged;
    ev.source  = id;
    ev.payload = static_cast<int>(AgentState::Idle);
    eventBus_.publish(ev);
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

float AgentSystem::getAverageSatisfaction(const entt::registry& reg) const
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

void AgentSystem::registerRestoredAgent(EntityId id) {
    activeAgents_.insert(static_cast<uint32_t>(id));
}

void AgentSystem::clearTracking() {
    activeAgents_.clear();
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

void AgentSystem::updateSatisfactionAndStress(entt::registry& reg, EntityId id,
                                              AgentComponent& agent,
                                              const AgentScheduleComponent& schedule)
{
    // Capture pre-change value for event emission
    const float prevSatisfaction = agent.satisfaction;

    // Apply satisfaction changes based on state
    switch (agent.state) {
        case AgentState::WaitingElevator:
            agent.satisfaction += SATISFACTION_LOSS_WAIT;
            agent.stress += STRESS_INCREASE_WAIT;
            break;
            
        case AgentState::Working:
            agent.satisfaction += SATISFACTION_GAIN_WORK;
            // Working state doesn't accumulate stress
            break;
            
        case AgentState::Idle:
        case AgentState::Resting:
            // Stress recovery during idle/resting
            agent.stress -= STRESS_DECREASE_IDLE;
            break;
            
        case AgentState::Leaving:
            // Agents in Leaving state will be despawned, no further updates
            return;
            
        default:
            // Other states don't affect satisfaction/stress
            break;
    }
    
    // Clamp satisfaction and stress to [0, 100]
    agent.satisfaction = std::clamp(agent.satisfaction, 0.0f, 100.0f);
    agent.stress       = std::clamp(agent.stress,       0.0f, 100.0f);

    // Emit AgentSatisfactionChanged when satisfaction changed after clamp (deferred)
    if (agent.satisfaction != prevSatisfaction) {
        Event ev;
        ev.type    = EventType::AgentSatisfactionChanged;
        ev.source  = id;
        ev.payload = static_cast<int>(agent.satisfaction);  // 0-100
        eventBus_.publish(ev);
    }

    // Check leave threshold
    if (agent.satisfaction < LEAVE_THRESHOLD && agent.state != AgentState::Leaving) {
        // Transition to Leaving state
        agent.state = AgentState::Leaving;
        
        // Emit AgentStateChanged event
        Event ev;
        ev.type   = EventType::AgentStateChanged;
        ev.source = id;
        ev.payload = static_cast<int>(AgentState::Leaving);
        eventBus_.publish(ev);
        
        spdlog::debug("AgentSystem: entity {:d} satisfaction {} < {}, transitioning to Leaving",
                      static_cast<uint32_t>(id), agent.satisfaction, LEAVE_THRESHOLD);
    }
}

void AgentSystem::updateTenantOccupantCount(entt::registry& reg, EntityId agentId,
                                            AgentState oldState, AgentState newState)
{
    // Working 상태 변경 시에만 처리
    bool wasWorking = (oldState == AgentState::Working);
    bool isWorking = (newState == AgentState::Working);
    
    if (wasWorking == isWorking) {
        return; // Working 상태 변경 없음
    }
    
    // 에이전트 컴포넌트 가져오기
    auto* agent = reg.try_get<AgentComponent>(agentId);
    if (!agent) {
        return;
    }
    
    // workplaceTenant 가져오기
    EntityId tenantId = agent->workplaceTenant;
    if (tenantId == INVALID_ENTITY) {
        return; // 직장이 없는 에이전트
    }
    
    // 테넌트 컴포넌트 가져오기
    auto* tenant = reg.try_get<TenantComponent>(tenantId);
    if (!tenant) {
        return; // 테넌트가 없음
    }
    
    // 점유자 수 업데이트
    if (isWorking) {
        // Working 상태 시작 → 점유자 증가
        tenant->occupantCount++;
        spdlog::debug("AgentSystem: tenant {} occupantCount++ → {}",
                      static_cast<uint32_t>(tenantId), tenant->occupantCount);
    } else {
        // Working 상태 종료 → 점유자 감소
        if (tenant->occupantCount > 0) {
            tenant->occupantCount--;
            spdlog::debug("AgentSystem: tenant {} occupantCount-- → {}",
                          static_cast<uint32_t>(tenantId), tenant->occupantCount);
        }
    }
}

} // namespace vse
