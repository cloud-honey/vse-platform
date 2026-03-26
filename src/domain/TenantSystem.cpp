#include "domain/TenantSystem.h"
#include "core/ContentRegistry.h"
#include "core/IAgentSystem.h"
#include <spdlog/spdlog.h>

namespace vse {

TenantSystem::TenantSystem(IGridSystem& grid, EventBus& bus, IEconomyEngine& economy)
    : grid_(grid), bus_(bus), economy_(economy)
{
    // TODO: evictionThreshold_를 balance.json에서 읽어오기
    // 현재는 하드코딩된 값 사용
    evictionThreshold_ = 20.0f;
}

Result<EntityId> TenantSystem::placeTenant(entt::registry& reg,
                                           TenantType type,
                                           TileCoord anchor,
                                           const ContentRegistry& content)
{
    // 1. balance.json에서 테넌트 속성 읽기
    // TODO: ContentRegistry에서 읽어오기 구현
    // 현재는 하드코딩된 값 사용
    int64_t rentPerDay = 0;
    int64_t maintenanceCost = 0;
    int maxOccupants = 0;
    int width = 1;
    int64_t buildCost = 0;
    float satisfactionDecayRate = 0.0f;

    switch (type) {
        case TenantType::Office:
            rentPerDay = 500;          // $5.00
            maintenanceCost = 100;     // $1.00
            maxOccupants = 6;
            width = 4;
            buildCost = 5000;          // $50.00
            satisfactionDecayRate = 0.1f;
            break;
        case TenantType::Residential:
            rentPerDay = 300;          // $3.00
            maintenanceCost = 50;      // $0.50
            maxOccupants = 3;
            width = 3;
            buildCost = 3000;          // $30.00
            satisfactionDecayRate = 0.05f;
            break;
        case TenantType::Commercial:
            rentPerDay = 800;          // $8.00
            maintenanceCost = 200;     // $2.00
            maxOccupants = 0;
            width = 5;
            buildCost = 8000;          // $80.00
            satisfactionDecayRate = 0.15f;
            break;
        default:
            return Result<EntityId>::failure(ErrorCode::InvalidArgument);
    }

    // 2. 엔티티 생성
    EntityId entity = reg.create();

    // 3. TenantComponent 부착
    TenantComponent& tenant = reg.emplace<TenantComponent>(entity);
    tenant.type = type;
    tenant.anchorTile = anchor;
    tenant.width = width;
    tenant.rentPerDay = rentPerDay;
    tenant.maintenanceCost = maintenanceCost;
    tenant.maxOccupants = maxOccupants;
    tenant.occupantCount = 0;
    tenant.buildCost = buildCost;
    tenant.satisfactionDecayRate = satisfactionDecayRate;
    tenant.isEvicted = false;
    tenant.evictionCountdown = 0;

    // 4. GridSystem에 테넌트 배치
    auto result = grid_.placeTenant(anchor, type, width, entity);
    if (!result.ok()) {
        reg.destroy(entity);
        return Result<EntityId>::failure(result.error);
    }

    // 5. 이벤트 발행 (지연)
    Event ev;
    ev.type = EventType::TenantPlaced;
    ev.source = entity;
    bus_.publish(ev);

    spdlog::debug("TenantSystem::placeTenant: type={}, anchor=({},{}), width={}, entity={}",
                  static_cast<int>(type), anchor.x, anchor.floor, width,
                  static_cast<uint32_t>(entity));

    return Result<EntityId>::success(entity);
}

void TenantSystem::removeTenant(entt::registry& reg, EntityId id)
{
    if (!reg.valid(id)) {
        spdlog::warn("TenantSystem::removeTenant: invalid entity {}", static_cast<uint32_t>(id));
        return;
    }

    // 1. TenantComponent 가져오기
    auto* tenant = reg.try_get<TenantComponent>(id);
    if (!tenant) {
        spdlog::warn("TenantSystem::removeTenant: entity {} has no TenantComponent",
                     static_cast<uint32_t>(id));
        reg.destroy(id);
        return;
    }

    // 2. GridSystem에서 테넌트 제거 (가능한 경우)
    // anchorTile이 유효한 좌표인지 확인
    if (grid_.isValidCoord(tenant->anchorTile)) {
        auto result = grid_.removeTenant(tenant->anchorTile);
        if (!result.ok()) {
            spdlog::warn("TenantSystem::removeTenant: grid.removeTenant failed for anchor=({},{})",
                         tenant->anchorTile.x, tenant->anchorTile.floor);
        }
    }

    // 3. 엔티티 제거
    reg.destroy(id);

    // 4. 이벤트 발행 (지연)
    Event ev;
    ev.type = EventType::TenantRemoved;
    ev.source = id;
    bus_.publish(ev);

    spdlog::debug("TenantSystem::removeTenant: entity {} removed", static_cast<uint32_t>(id));
}

void TenantSystem::onDayChanged(entt::registry& reg, const GameTime& time)
{
    auto view = reg.view<TenantComponent>();
    int tenantCount = 0;
    int64_t totalRent = 0;
    int64_t totalMaintenance = 0;

    for (auto entity : view) {
        auto& tenant = view.get<TenantComponent>(entity);

        // 퇴거 중인 테넌트는 임대료/유지비 처리 안 함
        if (tenant.isEvicted || tenant.evictionCountdown > 0) {
            continue;
        }

        // 임대료 수령
        if (tenant.rentPerDay > 0) {
            economy_.addIncome(entity, tenant.type, tenant.rentPerDay, time);
            totalRent += tenant.rentPerDay;
            Event ev;
            ev.type = EventType::RentCollected;
            ev.source = entity;
            bus_.publish(ev);
        }

        // 유지비 지불
        if (tenant.maintenanceCost > 0) {
            economy_.addExpense("tenant_maintenance", tenant.maintenanceCost, time);
            totalMaintenance += tenant.maintenanceCost;
        }

        tenantCount++;
    }

    if (tenantCount > 0) {
        spdlog::debug("TenantSystem::onDayChanged: day={}, tenants={}, rent=${:.2f}, maintenance=${:.2f}",
                      time.day, tenantCount,
                      totalRent / 100.0, totalMaintenance / 100.0);
    }
}

void TenantSystem::update(entt::registry& reg, const GameTime& time)
{
    auto view = reg.view<TenantComponent>();

    for (auto entity : view) {
        auto& tenant = view.get<TenantComponent>(entity);

        // 이미 퇴거된 테넌트는 건너뛰기
        if (tenant.isEvicted) {
            continue;
        }

        // 퇴거 카운트다운 중인 경우
        if (tenant.evictionCountdown > 0) {
            tenant.evictionCountdown--;

            // 카운트다운 종료 → 테넌트 제거
            if (tenant.evictionCountdown == 0) {
                tenant.isEvicted = true;
                spdlog::info("TenantSystem::update: tenant {} evicted (satisfaction too low)",
                             static_cast<uint32_t>(entity));
                removeTenant(reg, entity);
            }
            continue;
        }

        // 퇴거 조건 확인 (해당 테넌트에서 일하는 NPC 평균 만족도)
        // commercial 테넌트는 점유자가 없으므로 퇴거 조건 확인 안 함
        if (tenant.type == TenantType::Commercial || tenant.maxOccupants == 0) {
            continue;
        }

        float avgSatisfaction = computeAverageSatisfactionForTenant(reg, entity);
        if (avgSatisfaction < evictionThreshold_) {
            // 퇴거 카운트다운 시작 (60틱 = ~6초 at 10TPS, ~1 게임 시간)
            tenant.evictionCountdown = 60;
            spdlog::info("TenantSystem::update: tenant {} eviction countdown started (avg satisfaction={:.1f})",
                         static_cast<uint32_t>(entity), avgSatisfaction);
        }
    }
}

int TenantSystem::activeTenantCount(const entt::registry& reg) const
{
    auto view = reg.view<const TenantComponent>();
    int count = 0;
    
    for (auto entity : view) {
        const auto& tenant = view.get<const TenantComponent>(entity);
        if (!tenant.isEvicted && tenant.evictionCountdown == 0) {
            count++;
        }
    }
    
    return count;
}

float TenantSystem::computeAverageSatisfactionForTenant(const entt::registry& reg, EntityId tenantId) const
{
    float totalSatisfaction = 0.0f;
    int agentCount = 0;

    // 모든 에이전트 순회
    auto agentView = reg.view<AgentComponent>();
    for (auto agentEntity : agentView) {
        const auto& agent = agentView.get<AgentComponent>(agentEntity);

        // 이 테넌트에서 일하는 에이전트만 고려
        if (agent.workplaceTenant == tenantId && agent.state == AgentState::Working) {
            totalSatisfaction += agent.satisfaction;
            agentCount++;
        }
    }

    if (agentCount == 0) {
        return 100.0f; // 점유자가 없으면 만족도 최고로 간주
    }

    return totalSatisfaction / agentCount;
}

} // namespace vse