#include "renderer/RenderFrameCollector.h"
#include <algorithm>

namespace vse {

RenderFrameCollector::RenderFrameCollector(const IGridSystem& grid,
                                            const ITransportSystem& transport,
                                            int tileSizePx)
    : grid_(grid)
    , transport_(transport)
    , tileSizePx_(tileSizePx)
{}

RenderFrame RenderFrameCollector::collect() const
{
    RenderFrame frame;
    frame.maxFloors = grid_.maxFloors();
    frame.floorWidth = grid_.floorWidth();
    frame.tileSize = tileSizePx_;
    frame.drawGrid = drawGrid_;
    frame.drawDebugInfo = drawDebugInfo_;

    // ── 타일 수집 ────────────────────────────────────────
    // 건설된 층의 타일을 RenderTile로 변환
    for (int f = 0; f < frame.maxFloors; ++f) {
        if (!grid_.isFloorBuilt(f)) continue;

        for (int x = 0; x < frame.floorWidth; ++x) {
            auto coord = TileCoord{x, f};
            if (grid_.isTileEmpty(coord)) continue;

            // 점유된 타일 → TileData에서 테넌트 타입 추출
            auto tileOpt = grid_.getTile(coord);
            Color c;
            if (tileOpt.has_value()) {
                switch (tileOpt->tenantType) {
                case TenantType::Office:
                    c = Color::fromRGBA(70, 130, 180, 230);  // 파란
                    break;
                case TenantType::Residential:
                    c = Color::fromRGBA(60, 179, 113, 230);  // 초록
                    break;
                case TenantType::Commercial:
                    c = Color::fromRGBA(255, 165, 0, 230);   // 주황
                    break;
                default:
                    c = Color::fromRGBA(128, 128, 128, 230);
                    break;
                }
                // 엘리베이터 샤프트 타일
                if (tileOpt->isElevatorShaft) {
                    c = Color::fromRGBA(100, 100, 120, 200);
                }
            } else {
                c = Color::fromRGBA(128, 128, 128, 230);
            }

            frame.tiles.push_back(RenderTile{x, f, c});
        }
    }

    // 로비 (0층) — 빈 타일 특수 색상
    if (grid_.isFloorBuilt(0)) {
        for (int x = 0; x < frame.floorWidth; ++x) {
            auto coord = TileCoord{x, 0};
            if (!grid_.isTileEmpty(coord)) continue;  // 이미 추가된 건 스킵
            frame.tiles.push_back(RenderTile{x, 0,
                Color::fromRGBA(80, 80, 100, 150)});
        }
    }

    // ── 엘리베이터 수집 ──────────────────────────────────
    auto elevIds = transport_.getAllElevators();
    for (auto eid : elevIds) {
        auto snap = transport_.getElevatorState(eid);
        if (!snap.has_value()) continue;

        RenderElevator re;
        re.shaftX             = snap->shaftX;
        re.currentFloor       = snap->currentFloor;
        re.interpolatedFloor  = snap->interpolatedFloor;
        re.state              = snap->state;
        re.direction          = snap->direction;
        re.passengerCount     = snap->passengerCount;
        re.capacity           = snap->capacity;

        // 상태별 색상
        switch (snap->state) {
        case ElevatorState::Idle:
            re.color = Color::fromRGBA(120, 120, 140, 220);
            break;
        case ElevatorState::MovingUp:
        case ElevatorState::MovingDown:
            re.color = Color::fromRGBA(79, 142, 247, 220);
            break;
        case ElevatorState::DoorOpening:
        case ElevatorState::Boarding:
            re.color = Color::fromRGBA(46, 204, 138, 220);
            break;
        case ElevatorState::DoorClosing:
            re.color = Color::fromRGBA(255, 200, 60, 220);
            break;
        }

        frame.elevators.push_back(re);
    }

    // ── 에이전트 수집 (TASK-01-008) ──────────────────────
    // PositionComponent.pixel(발바닥 월드좌표) + AgentComponent.state → RenderAgentCmd
    // const view 사용 — 컴포넌트 수정 불가 (Gemini 검토 반영)
    if (agentSys_ != nullptr && registry_ != nullptr) {
        auto view = registry_->view<const PositionComponent, const AgentComponent>();
        frame.agents.reserve(view.size_hint());  // 매 프레임 재할당 방지 (Gemini 검토 반영)
        for (auto entity : view) {
            const auto& pos   = view.get<const PositionComponent>(entity);
            const auto& agent = view.get<const AgentComponent>(entity);

            RenderAgentCmd ra;
            ra.id          = entity;
            ra.pos         = pos.pixel;   // pixel = 발바닥 월드 픽셀 (PixelPos 계약)
            ra.facing      = pos.facing;
            ra.state       = agent.state;
            ra.spriteFrame = 0;           // Phase 1 고정
            frame.agents.push_back(ra);
        }
        // Y-sort: pos.y 오름차순 정렬 → 낮은 층(y 작음) 먼저 그려서 높은 층이 앞에 보임
        // (Gemini 검토 반영 — 탑뷰 Y-sorting)
        std::sort(frame.agents.begin(), frame.agents.end(),
                  [](const RenderAgentCmd& a, const RenderAgentCmd& b) {
                      return a.pos.y < b.pos.y;
                  });
    }

    return frame;
}

} // namespace vse
