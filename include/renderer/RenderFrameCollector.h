#pragma once
#include "core/IRenderCommand.h"
#include "core/IGridSystem.h"
#include "core/ITransportSystem.h"
#include "core/IAgentSystem.h"
#include <entt/entt.hpp>

namespace vse {

/**
 * RenderFrameCollector — Domain 상태 → RenderFrame 수집.
 *
 * Domain 시스템을 읽기 전용으로 참조하여
 * 매 프레임 RenderFrame을 생성한다.
 * Domain은 이 클래스를 모르며, 이 클래스가 Domain에 의존한다.
 *
 * Layer 3 소속 — 게임 로직 계산 금지.
 *
 * AgentSystem 연결 (TASK-01-008):
 * - setAgentSource()로 IAgentSystem + entt::registry 연결
 * - collect() 시 PositionComponent.pixel → RenderAgent 변환
 * - AgentSystem 미연결 시 agents 빈 배열 (정상 동작)
 */
class RenderFrameCollector {
public:
    RenderFrameCollector(const IGridSystem& grid, const ITransportSystem& transport,
                         int tileSizePx = 32);

    // AgentSystem 연결 (선택) — collect() 시 에이전트 수집
    // registry는 const 포인터 — 읽기 전용 접근만 허용 (entt const view 사용)
    void setAgentSource(const IAgentSystem* agentSys, const entt::registry* reg) {
        agentSys_ = agentSys;
        registry_ = reg;
    }

    // 현재 도메인 상태를 기반으로 RenderFrame 생성
    RenderFrame collect() const;

    // 디버그 플래그
    void setDrawGrid(bool v)      { drawGrid_ = v; }
    void setDrawDebugInfo(bool v)  { drawDebugInfo_ = v; }

private:
    const IGridSystem&          grid_;
    const ITransportSystem&     transport_;
    const IAgentSystem*         agentSys_ = nullptr;
    const entt::registry*       registry_ = nullptr;  // const — 읽기 전용
    int  tileSizePx_    = 32;
    bool drawGrid_      = true;
    bool drawDebugInfo_ = true;
};

} // namespace vse
