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
                                           const ContentRegistry& content,
                                           const GameTime& time)
{
    // 1. balance.json에서 테넌트 속성 읽기
    const auto& bd = content.getBalanceData();
    int64_t rentPerDay = 0;
    int64_t maintenanceCost = 0;
    int maxOccupants = 0;
    int width = 1;
    int64_t buildCost = 0;
    float satisfactionDecayRate = 0.0f;

    switch (type) {
        case TenantType::Office:
            rentPerDay          = bd.value("tenants.office.rent",                   500LL);
            maintenanceCost     = bd.value("tenants.office.maintenance",            100LL);
            maxOccupants        = bd.value("tenants.office.maxOccupants",           6);
            width               = bd.value("tenants.office.width",                  4);
            buildCost           = bd.value("tenants.office.buildCost",              5000LL);
            satisfactionDecayRate = bd.value("tenants.office.satisfactionDecayRate", 0.1f);
            break;
        case TenantType::Residential:
            rentPerDay          = bd.value("tenants.residential.rent",              300LL);
            maintenanceCost     = bd.value("tenants.residential.maintenance",       50LL);
            maxOccupants        = bd.value("tenants.residential.maxOccupants",      3);
            width               = bd.value("tenants.residential.width",             3);
            buildCost           = bd.value("tenants.residential.buildCost",         3000LL);
            satisfactionDecayRate = bd.value("tenants.residential.satisfactionDecayRate", 0.05f);
            break;
        case TenantType::Commercial:
            rentPerDay          = bd.value("tenants.commercial.rent",               800LL);
            maintenanceCost     = bd.value("tenants.commercial.maintenance",        200LL);
            maxOccupants        = bd.value("tenants.commercial.maxOccupants",       0);
            width               = bd.value("tenants.commercial.width",              5);
            buildCost           = bd.value("tenants.commercial.buildCost",          8000LL);
            satisfactionDecayRate = bd.value("tenants.commercial.satisfactionDecayRate", 0.15f);
            break;
        default:
            return Result<EntityId>::failure(ErrorCode::InvalidArgument);
    }

    // 2. 잔액 확인 — 부족하면 InsufficientFunds 이벤트 발행 후 실패
    int64_t balance = economy_.getBalance();
    if (balance < buildCost) {
        Event ev;
        ev.type = EventType::InsufficientFunds;
        ev.payload = InsufficientFundsPayload{"placeTenant", buildCost, balance};
        bus_.publish(ev);
        spdlog::warn("TenantSystem::placeTenant: insufficient funds (required={}, available={})",
                     buildCost, balance);
        return Result<EntityId>::failure(ErrorCode::InsufficientFunds);
    }

    // 3. 엔티티 생성 + TenantComponent 부착
    EntityId entity = reg.create();

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

    // 5. 배치 성공 후 비용 차감 (성공 확인 후에만 차감)
    economy_.addExpense("tenant_build", buildCost, time);

    // 6. 이벤트 발행 (지연)
    Event ev;
    ev.type = EventType::TenantPlaced;
    ev.source = entity;
    bus_.publish(ev);

    spdlog::debug("TenantSystem::placeTenant: type={}, anchor=({},{}), width={}, entity={}, cost={}",
                  static_cast<int>(type), anchor.x, anchor.floor, width,
                  static_cast<uint32_t>(entity), buildCost);

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
    // 임대료: IEconomyEngine::collectRent에 위임 (GridSystem 기반 계산)
    economy_.collectRent(grid_, time);

    // 엘리베이터 유지비: IEconomyEngine::payMaintenance에 위임
    economy_.payMaintenance(grid_, time);

    // 테넌트별 유지비: TenantComponent 기반으로 직접 처리 (EconomyEngine은 엘리베이터만 처리)
    auto view = reg.view<TenantComponent>();
    int tenantCount = 0;

    for (auto entity : view) {
        const auto& tenant = view.get<TenantComponent>(entity);
        if (tenant.isEvicted || tenant.evictionCountdown > 0) continue;

        if (tenant.maintenanceCost > 0) {
            economy_.addExpense("tenant_maintenance", tenant.maintenanceCost, time);
        }

        Event ev;
        ev.type   = EventType::RentCollected;
        ev.source = entity;
        bus_.publish(ev);
        tenantCount++;
    }

    if (tenantCount > 0) {
        spdlog::debug("TenantSystem::onDayChanged: day={}, tenants={} (rent/maintenance delegated to EconomyEngine)",
                      time.day, tenantCount);
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

        // commercial 테넌트는 점유자가 없으므로 퇴거 조건 확인 안 함
        if (tenant.type == TenantType::Commercial || tenant.maxOccupants == 0) {
            continue;
        }

        float avgSatisfaction = computeAverageSatisfactionForTenant(reg, entity);

        if (tenant.evictionCountdown > 0) {
            // 카운트다운 중 — 만족도 회복 시 리셋
            if (avgSatisfaction >= evictionThreshold_) {
                tenant.evictionCountdown = 0;
                spdlog::info("TenantSystem::update: tenant {} eviction cancelled (satisfaction recovered to {:.1f})",
                             static_cast<uint32_t>(entity), avgSatisfaction);
            } else {
                tenant.evictionCountdown--;
                if (tenant.evictionCountdown == 0) {
                    tenant.isEvicted = true;
                    spdlog::info("TenantSystem::update: tenant {} evicted (satisfaction too low)",
                                 static_cast<uint32_t>(entity));
                    removeTenant(reg, entity);
                }
            }
        } else {
            // 퇴거 조건 확인
            if (avgSatisfaction < evictionThreshold_) {
                tenant.evictionCountdown = 60;
                spdlog::info("TenantSystem::update: tenant {} eviction countdown started (avg satisfaction={:.1f})",
                             static_cast<uint32_t>(entity), avgSatisfaction);
            }
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