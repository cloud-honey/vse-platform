#pragma once
#include <cstdint>

namespace vse {

/**
 * InputTypes — Layer 3에서 생성, Domain에서 소비하는 게임 커맨드.
 *
 * 흐름: SDL_Event → InputMapper → GameCommand → commandQueue → processCommands()
 *
 * 설계 원칙:
 * - Domain은 SDL_Event를 직접 보지 않음
 * - GameCommand는 값 타입 (큐 복사 안전)
 * - Phase 1: 건설/카메라/시스템 커맨드만
 */

enum class CommandType : uint16_t {
    // ── 건설 ──────────────
    BuildFloor = 100,       // payload: floor number
    PlaceTenant,            // payload: x, floor, tenantType, width
    PlaceElevatorShaft,     // payload: shaftX, bottomFloor, topFloor
    CreateElevator,         // payload: shaftX, bottomFloor, topFloor, capacity

    // ── 카메라 ────────────
    CameraPan = 200,        // payload: deltaX, deltaY (px)
    CameraZoom,             // payload: zoomDelta
    CameraReset,            // 카메라 초기 위치로

    // ── 시스템 ────────────
    TogglePause = 300,
    SetSpeed,               // payload: speedMultiplier (1, 2, 4)
    ToggleDebugOverlay,
    Quit,

    // ── 선택 ──────────────
    SelectTile = 400,       // payload: tileX, tileFloor
    Deselect,
};

/**
 * GameCommand — 하나의 게임 커맨드.
 *
 * 다양한 커맨드 타입의 payload를 union으로 담는다.
 * 활성 멤버는 type에 의해 결정된다.
 */
struct GameCommand {
    CommandType type;

    union {
        struct { int floor; }                           buildFloor;
        struct { int x; int floor; int tenantType; int width; } placeTenant;
        struct { int shaftX; int bottomFloor; int topFloor; }   placeShaft;
        struct { int shaftX; int bottomFloor; int topFloor; int capacity; } createElevator;
        struct { float deltaX; float deltaY; }          cameraPan;
        struct { float zoomDelta; }                     cameraZoom;
        struct { int speedMultiplier; }                 setSpeed;
        struct { int tileX; int tileFloor; }            selectTile;
    };

    // 편의 생성자
    static GameCommand makeBuildFloor(int floor) {
        GameCommand cmd{};
        cmd.type = CommandType::BuildFloor;
        cmd.buildFloor.floor = floor;
        return cmd;
    }

    static GameCommand makePlaceTenant(int x, int floor, int tenantType, int width) {
        GameCommand cmd{};
        cmd.type = CommandType::PlaceTenant;
        cmd.placeTenant = {x, floor, tenantType, width};
        return cmd;
    }

    static GameCommand makeCameraPan(float dx, float dy) {
        GameCommand cmd{};
        cmd.type = CommandType::CameraPan;
        cmd.cameraPan = {dx, dy};
        return cmd;
    }

    static GameCommand makeCameraZoom(float delta) {
        GameCommand cmd{};
        cmd.type = CommandType::CameraZoom;
        cmd.cameraZoom = {delta};
        return cmd;
    }

    static GameCommand makeTogglePause() {
        GameCommand cmd{};
        cmd.type = CommandType::TogglePause;
        return cmd;
    }

    static GameCommand makeSetSpeed(int multiplier) {
        GameCommand cmd{};
        cmd.type = CommandType::SetSpeed;
        cmd.setSpeed = {multiplier};
        return cmd;
    }

    static GameCommand makeQuit() {
        GameCommand cmd{};
        cmd.type = CommandType::Quit;
        return cmd;
    }

    static GameCommand makeSelectTile(int x, int floor) {
        GameCommand cmd{};
        cmd.type = CommandType::SelectTile;
        cmd.selectTile = {x, floor};
        return cmd;
    }
};

// BuildModeState — 렌더러 빌드 모드 상태 (RenderFrame 전달용)
// Layer 0 데이터 계약 (값 타입, 코드 로직 없음)
enum class BuildAction { None, BuildFloor, PlaceTenant };

struct BuildModeState {
    BuildAction action     = BuildAction::None;
    int         tenantType  = 0;   // 0=Office, 1=Residential, 2=Commercial
    int         tenantWidth = 1;
    bool        active      = false;
};

} // namespace vse
