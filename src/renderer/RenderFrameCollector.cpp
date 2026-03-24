#include "renderer/RenderFrameCollector.h"

namespace vse {

RenderFrameCollector::RenderFrameCollector(const IGridSystem& grid,
                                            const ITransportSystem& transport)
    : grid_(grid)
    , transport_(transport)
{}

RenderFrame RenderFrameCollector::collect() const
{
    RenderFrame frame;
    frame.maxFloors = grid_.maxFloors();
    frame.floorWidth = grid_.floorWidth();
    frame.tileSize = 32;  // ConfigManager에서 읽어야 하지만 Phase 1은 32 고정
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

    return frame;
}

} // namespace vse
