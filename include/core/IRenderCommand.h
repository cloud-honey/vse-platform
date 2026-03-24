#pragma once
#include "core/Types.h"
#include <vector>
#include <cstdint>

namespace vse {

/**
 * IRenderCommand — Layer 3 렌더링을 위한 커맨드 구조체.
 *
 * Domain (Layer 1)이 매 프레임 RenderFrame을 수집하면
 * SDLRenderer (Layer 3)가 이를 소비하여 화면에 그린다.
 *
 * 설계 원칙:
 * - RenderFrame은 값 타입 (복사 가능)
 * - Domain 시스템은 SDL2를 직접 참조하지 않음
 * - Phase 1: 컬러 타일만 (스프라이트는 Phase 2)
 */

// ── 색상 ──────────────────────────────────────────────────
struct Color {
    uint8_t r, g, b, a;

    static constexpr Color fromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return {r, g, b, a};
    }
};

// ── 개별 렌더 커맨드 ──────────────────────────────────────
/**
 * RenderTile — 타일 1칸을 특정 색으로 그리라는 커맨드.
 * Phase 1: 컬러 박스. Phase 2: spriteId 추가 예정.
 */
struct RenderTile {
    int   x;
    int   floor;
    Color color;
    // [PHASE-2] uint16_t spriteId = 0;
};

/**
 * RenderElevator — 엘리베이터 1대의 렌더링 정보.
 * Phase 1: 컬러 박스 + 층 위치. Phase 3: floatFloor 보간.
 */
struct RenderElevator {
    int             shaftX;
    int             currentFloor;
    float           interpolatedFloor;  // Phase 1: == currentFloor
    ElevatorState   state;
    ElevatorDirection direction;
    int             passengerCount;
    int             capacity;
    Color           color;
};

/**
 * RenderText — 텍스트 오버레이 (디버그/HUD).
 */
struct RenderText {
    float       screenX;    // 화면 좌표 (px)
    float       screenY;
    std::string text;
    Color       color;
    int         fontSize;   // px
};

// ── 프레임 데이터 ─────────────────────────────────────────
/**
 * RenderFrame — 1 프레임에 그려야 할 전체 데이터.
 * Domain → RenderFrame 수집 → SDLRenderer 소비.
 */
struct RenderFrame {
    // 카메라 상태 (SDLRenderer가 좌표 변환에 사용)
    float cameraX      = 0.0f;
    float cameraY      = 0.0f;
    float cameraZoom   = 1.0f;
    int   viewportW    = 1280;
    int   viewportH    = 720;

    // 그리드 정보
    int   maxFloors    = 30;
    int   floorWidth   = 40;
    int   tileSize     = 32;

    // 렌더 커맨드 목록
    std::vector<RenderTile>     tiles;
    std::vector<RenderElevator> elevators;
    std::vector<RenderText>     texts;

    // 디버그 플래그
    bool drawGrid      = true;   // 그리드 선 표시
    bool drawDebugInfo = true;   // FPS, 틱 정보 등
};

} // namespace vse
