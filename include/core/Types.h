#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <entt/entt.hpp>

namespace vse {

// ── Coordinate ──────────────────────────────
// Origin: bottom-left. Y increases upward. floor 0 = ground.
struct TileCoord {
    int x;
    int floor;      // 0 = ground, negative = basement (Phase 2)

    bool operator==(const TileCoord& o) const { return x == o.x && floor == o.floor; }
    bool operator!=(const TileCoord& o) const { return !(*this == o); }
};

} // namespace vse — temporarily close for hash specialization

// std::hash specialization for TileCoord (needed for unordered_map keys)
template<>
struct std::hash<vse::TileCoord> {
    size_t operator()(const vse::TileCoord& c) const noexcept {
        return std::hash<int>()(c.x) ^ (std::hash<int>()(c.floor) << 16);
    }
};

namespace vse { // reopen

// Pixel position — **world-space coordinates** (bottom-left origin, Y increases upward).
// Same coordinate system as tile grid:
//   pixelX = tile.x * config.tileSize
//   pixelY = tile.floor * config.tileSize   (NOT render space)
// Camera::worldToScreenX/Y() converts to SDL2 screen space.
// PositionComponent.pixel uses this convention (foot position = bottom of sprite).
struct PixelPos {
    float x;
    float y;
};

// ── Entity IDs ──────────────────────────────
// Phase 1: EntityId = entt::entity directly. No separate stable ID layer.
// Stable ID for save/load is deferred to Phase 2 SaveLoad detailed design.
using EntityId = entt::entity;
constexpr EntityId INVALID_ENTITY = entt::null;

// ── Time ────────────────────────────────────
using SimTick = uint64_t;        // Monotonic tick counter
using GameMinute = uint32_t;     // In-game minutes since day 0

struct GameTime {
    int day;         // 0-indexed
    int hour;        // 0-23
    int minute;      // 0-59

    GameMinute toMinutes() const { return day * 1440 + hour * 60 + minute; }

    static GameTime fromTick(SimTick tick) {
        int totalMin = static_cast<int>(tick);  // 1 tick = 1 game minute
        return GameTime{
            totalMin / 1440,
            (totalMin % 1440) / 60,
            totalMin % 60
        };
    }
};

// ── Enums ───────────────────────────────────
enum class TenantType : uint8_t {
    Office = 0,
    Residential,
    Commercial,
    COUNT           // Always last
};

enum class AgentState : uint8_t {
    Idle = 0,
    Moving,
    Working,
    Resting,
    WaitingElevator,
    InElevator,
    UsingStairs,    // Moving via stairs
    Leaving,        // Dissatisfied, leaving building
    COUNT
};

enum class Direction : uint8_t {
    Left = 0,
    Right,
    Up,             // Stairs/elevator
    Down
};

enum class ElevatorDirection : uint8_t {
    Idle = 0,
    Up,
    Down
};

// Elevator FSM 상태 (authoritative)
// Idle → MovingUp/MovingDown → DoorOpening → Boarding → DoorClosing → (Idle or Moving)
enum class ElevatorState : uint8_t {
    Idle = 0,
    MovingUp,
    MovingDown,
    DoorOpening,
    Boarding,
    DoorClosing
};

enum class StarRating : uint8_t {
    Star0 = 0,
    Star1,
    Star2,
    Star3,
    Star4,
    Star5
};

// ── Event Payloads (shared) ─────────────────

/** Payload for StarRatingChanged event. Moved from StarRatingSystem.h (TASK-02-009). */
struct StarRatingChangedPayload {
    StarRating oldRating;
    StarRating newRating;
    float avgSatisfaction;
};

/** Payload for InsufficientFunds event. */
struct InsufficientFundsPayload {
    std::string action;
    int64_t required;
    int64_t available;
};

/** Payload for DailySettlement event. */
struct DailySettlementPayload {
    int day;
    int64_t income;
    int64_t expense;
    int64_t balance;
};

/** Payload for WeeklyReport event. */
struct WeeklyReportPayload {
    int weekNumber;       // day / 7
    int64_t weeklyIncome;
    int64_t weeklyExpense;
};

/** Payload for QuarterlySettlement event. */
struct QuarterlySettlementPayload {
    int quarter;          // day / 90
    int64_t taxAmount;    // 10% of quarterly income
    int64_t balance;
};

// ── Event Types ─────────────────────────────
enum class EventType : uint16_t {
    // SimClock
    TickAdvanced = 100,
    DayChanged,
    HourChanged,

    // Grid
    TileOccupied = 200,
    TileVacated,
    FloorBuilt,

    // Agent
    AgentSpawned = 300,
    AgentDespawned,
    AgentMoved,
    AgentStateChanged,
    AgentSatisfactionChanged,

    // Transport
    ElevatorCalled = 400,
    ElevatorArrived,
    ElevatorBoarded,
    ElevatorExited,

    // Economy
    IncomeReceived = 500,
    ExpensePaid,
    RentCollected,
    BalanceChanged,

    // Economy Errors
    InsufficientFunds = 800,

    // Building
    TenantPlaced = 600,
    TenantRemoved,
    StarRatingChanged,

    // Input/Command — notification AFTER command is processed (for UI feedback/logging, not for command delivery)
    GameCommandIssued = 700,

    // Settlement Events
    DailySettlement = 550,
    WeeklyReport = 551,
    QuarterlySettlement = 552,
    StarRatingReEvalRequested = 553,  // Quarterly: trigger StarRatingSystem re-evaluation

    // System
    GameSaved = 900,
    GameLoaded,
    ConfigReloaded,
};

// ── Constants ───────────────────────────────
// Role: fallback values when game_config.json fails to load.
namespace defaults {
    constexpr int MAX_FLOORS = 30;
    constexpr int TILE_SIZE = 32;
    constexpr int ELEVATOR_CAPACITY = 8;
    constexpr int INITIAL_MONEY = 100000;
    constexpr int TICKS_PER_SECOND = 60;   // Fixed-tick loop frequency
}

} // namespace vse