#pragma once
#include "core/IRenderCommand.h"
#include "core/IGridSystem.h"
#include "core/ITransportSystem.h"

namespace vse {

/**
 * RenderFrameCollector — Domain 상태 → RenderFrame 수집.
 *
 * Domain 시스템을 읽기 전용으로 참조하여
 * 매 프레임 RenderFrame을 생성한다.
 * Domain은 이 클래스를 모르며, 이 클래스가 Domain에 의존한다.
 *
 * Layer 3 소속 — 게임 로직 계산 금지.
 */
class RenderFrameCollector {
public:
    RenderFrameCollector(const IGridSystem& grid, const ITransportSystem& transport,
                         int tileSizePx = 32);

    // 현재 도메인 상태를 기반으로 RenderFrame 생성
    RenderFrame collect() const;

    // 디버그 플래그
    void setDrawGrid(bool v)      { drawGrid_ = v; }
    void setDrawDebugInfo(bool v)  { drawDebugInfo_ = v; }

private:
    const IGridSystem&      grid_;
    const ITransportSystem& transport_;
    int  tileSizePx_    = 32;
    bool drawGrid_      = true;
    bool drawDebugInfo_ = true;
};

} // namespace vse
