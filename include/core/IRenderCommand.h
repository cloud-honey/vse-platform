#pragma once
#include "core/Types.h"
#include "core/InputTypes.h"
#include <vector>
#include <cstdint>
#include <string>

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
 * RenderAgentCmd — NPC 에이전트 1명의 렌더링 커맨드.
 *
 * Design Spec RenderAgentCmd 정의와 일치.
 *
 * pos: NPC 발바닥 월드 픽셀 위치 (PixelPos — 월드 좌하단 기준, Y↑).
 *      SDLRenderer가 Camera::worldToScreenY() 후 보정(drawY = sy - npcH)해 그림.
 * facing: 방향 (Left/Right) — Phase 2 스프라이트 플립 결정.
 * state: 행동 상태 — Phase 1 색상 결정 (Idle=회색, Working=파랑, Resting=주황).
 * spriteFrame: Phase 1에서는 항상 0. Phase 2 애니메이션 확장용 슬롯.
 */
struct RenderAgentCmd {
    EntityId    id          = INVALID_ENTITY;
    PixelPos    pos;                            // NPC 발바닥 월드 픽셀 (좌하단 원점, Y↑)
    Direction   facing      = Direction::Right;
    AgentState  state       = AgentState::Idle;
    int         spriteFrame = 0;                // Phase 1: 0 고정. Phase 2: 애니메이션 프레임
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

/**
 * DebugInfo — 디버그 패널에 표시할 정보.
 */
struct DebugInfo {
    int     gameTick        = 0;
    int     gameHour        = 0;
    int     gameMinute      = 0;
    int     gameDay         = 0;  // 0-indexed (GameTime.day와 동일 규칙 — Types.h)
    float   simSpeed        = 1.0f;
    bool    isPaused        = false;
    int     npcTotal        = 0;
    int     npcIdle         = 0;
    int     npcWorking      = 0;
    int     npcResting      = 0;
    int     elevatorCount   = 0;
    float   avgSatisfaction = 100.0f;
    float   fps             = 0.0f;
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
    std::vector<RenderTile>         tiles;
    std::vector<RenderElevator>     elevators;
    std::vector<RenderAgentCmd>     agents;     // Y-sorted by pos.y ascending before render
    std::vector<RenderText>         texts;

    // 디버그 플래그
    bool drawGrid      = true;   // 그리드 선 표시
    bool drawDebugInfo = true;   // FPS, 틱 정보 등

    // HUD 데이터
    int64_t balance      = 0;       // Current player balance in KRW
    float   starRating   = 0.0f;    // 0.0–5.0 (TOWER = 5.0)
    int     currentTick  = 0;       // Current simulation tick
    int     tenantCount  = 0;       // Occupied tenants
    int     npcCount     = 0;       // Active NPCs
    bool    showHUD      = true;    // Toggle HUD visibility
    int64_t dailyIncome  = 0;       // Daily income (Cents)
    int64_t dailyExpense = 0;       // Daily expense (Cents)

    // 디버그 정보
    DebugInfo debug;

    // 마우스 위치 (BuildCursor용)
    int mouseX = 0;
    int mouseY = 0;

    // Build mode 상태 (건설 모드 피드백용)
    BuildModeState buildMode;

    // Game state (for UI menus)
    GameState gameState = GameState::MainMenu;
};

} // namespace vse
