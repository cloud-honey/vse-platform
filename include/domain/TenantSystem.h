#pragma once
#include "core/IGridSystem.h"
#include "core/EventBus.h"
#include "core/IEconomyEngine.h"
#include "core/ContentRegistry.h"
#include <entt/entt.hpp>

namespace vse {

/**
 * TenantSystem — 테넌트(사무실/주거/상업)의 수명주기 관리.
 * - 테넌트 배치/제거
 * - 임대료 수령 및 유지비 지불
 * - NPC 점유 수 추적
 * - 만족도 저하 시 퇴거 처리
 *
 * Layer: domain/ (게임 규칙)
 * Fixed-tick only — dt 파라미터 없음.
 */
class TenantSystem {
public:
    TenantSystem(IGridSystem& grid, EventBus& bus, IEconomyEngine& economy);

    /**
     * 새 테넌트 배치.
     * - 엔티티 생성 + TenantComponent 부착
     * - GridSystem.placeTenant() 호출
     * - balance.json에서 rent/maintenance/buildCost/maxOccupants 읽기
     */
    Result<EntityId> placeTenant(entt::registry& reg,
                                 TenantType type,
                                 TileCoord anchor,
                                 const ContentRegistry& content);

    /**
     * 테넌트 제거.
     * - 엔티티 제거
     * - GridSystem.removeTenant() 호출 (가능한 경우)
     */
    void removeTenant(entt::registry& reg, EntityId id);

    /**
     * 일일 업데이트 (DayChanged 이벤트 핸들러).
     * - 모든 테넌트의 임대료 수령
     * - 모든 테넌트의 유지비 지불
     */
    void onDayChanged(entt::registry& reg, const GameTime& time);

    /**
     * 틱 업데이트.
     * - 퇴거 조건 확인 (해당 테넌트에서 일하는 NPC 평균 만족도 < evictionThreshold)
     * - 퇴거 카운트다운 감소, 0 도달 시 removeTenant() 호출
     */
    void update(entt::registry& reg, const GameTime& time);

    /**
     * 활성 테넌트 수 반환.
     */
    int activeTenantCount(const entt::registry& reg) const;

private:
    IGridSystem& grid_;
    EventBus& bus_;
    IEconomyEngine& economy_;
    float evictionThreshold_ = 20.0f; // balance.json에서 읽어올 값

    /**
     * 해당 테넌트에서 일하는 NPC들의 평균 만족도 계산.
     */
    float computeAverageSatisfactionForTenant(const entt::registry& reg, EntityId tenantId) const;
};

} // namespace vse