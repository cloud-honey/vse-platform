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
                    c = Color::fromRGBA(90, 150, 210, 240);  // 밝은 파랑 (TASK-02-008)
                    break;
                case TenantType::Residential:
                    c = Color::fromRGBA(80, 200, 130, 240);  // 밝은 초록 (TASK-02-008)
                    break;
                case TenantType::Commercial:
                    c = Color::fromRGBA(255, 180, 40, 240);  // 밝은 노란-주황 (TASK-02-008)
                    break;
                default:
                    c = Color::fromRGBA(160, 160, 170, 230); // 밝은 회색 (TASK-02-008)
                    break;
                }
                // 엘리베이터 샤프트 타일 — 더 밝은 메탈릭 그레이 (TASK-02-008)
                if (tileOpt->isElevatorShaft) {
                    c = Color::fromRGBA(130, 130, 160, 220);
                }
            } else {
                c = Color::fromRGBA(160, 160, 170, 230);     // 밝은 회색 (TASK-02-008)
            }

            RenderTile rt;
            rt.x = x;
            rt.floor = f;
            rt.color = c;
            if (tileOpt.has_value()) {
                rt.tenantType = tileOpt->tenantType;
                rt.isElevatorShaft = tileOpt->isElevatorShaft;
            } else {
                rt.tenantType = TenantType::None;
                rt.isElevatorShaft = false;
            }
            rt.isLobby = (f == 0);
            frame.tiles.push_back(rt);
        }
    }

    // 로비 (0층) — 빈 타일 특수 색상 — 어두운 배경 대비 구별 (TASK-02-008)
    if (grid_.isFloorBuilt(0)) {
        for (int x = 0; x < frame.floorWidth; ++x) {
            auto coord = TileCoord{x, 0};
            if (!grid_.isTileEmpty(coord)) continue;  // 이미 추가된 건 스킵
            RenderTile rt;
            rt.x = x;
            rt.floor = 0;
            rt.color = Color::fromRGBA(70, 75, 90, 200);
            rt.tenantType = TenantType::None;
            rt.isElevatorShaft = false;
            rt.isLobby = true;
            frame.tiles.push_back(rt);
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
