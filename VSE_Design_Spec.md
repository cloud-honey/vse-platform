# VSE Design Specification — Phase 1
> Version: 1.3 | Author: 붐 (PM) | Date: 2026-03-24
> Source of truth: `CLAUDE.md` v1.2 | This document details HOW to implement what CLAUDE.md defines.
> CLAUDE.md = WHAT (rules, constraints) | This doc = HOW (signatures, data flow, init order)
> v1.1: Cross-review 11 items applied (Layer 0 split, EntityId=entt, tick-time rules, deferred EventBus, GameCommand, grid occupancy, elevator FSM, save/load scope, consistency fixes, defaults policy, test coverage)
> v1.2: Final review 6 items — true fixed-tick loop, CLAUDE.md sync, EnTT snapshot save/restore, Bootstrapper=composition root, config immutable ticks, document consistency

---

## 0. Layer Definition (Authoritative)

Layer 0 is split into two parts:

| Sub-layer | Location | Contains | Rules |
|---|---|---|---|
| **Core API** | `include/core/` | `I*.h` = pure virtual interfaces. `Types.h`/`Error.h` = POD/enums. Other `.h` = Core Runtime service **declarations** (concrete class, but header only — impl in `src/core/`). | `I*.h`: no concrete impl. Other headers: no game-specific rules. |
| **Core Runtime** | `src/core/` | Shared runtime services: `SimClock`, `EventBus`, `ConfigManager`, `ContentRegistry`, `LocaleManager`, `Bootstrapper` | Generic infrastructure only. **No game-specific rules.** A "building satisfaction formula" belongs in Layer 1, not here. |

**Layer 1** (`domain/`) = Game-specific logic. RealEstateDomain. Only layer allowed to contain game rules.
**Layer 2** (`content/`) = JSON + sprites. No code logic beyond loading/parsing.
**Layer 3** (`renderer/`) = SDL2 + ImGui. Consumes `RenderFrame`, emits `GameCommand`. **Never mutates domain state directly.**

> This resolves the v1.0 inconsistency where `src/core/` was labeled "Layer 0 implementations" while CLAUDE.md defines Layer 0 as "interfaces only, no concrete implementation." The distinction is: Core API = contracts, Core Runtime = plumbing (no game rules).

---

## 1. Directory Structure & File Map

```
vse-platform/
├── CMakeLists.txt                    # Root build
├── cmake/
│   └── FetchDependencies.cmake       # All FetchContent declarations
│
├── include/
│   ├── core/                         # Layer 0 Core API — interfaces, types, contracts
│   │   ├── IGridSystem.h
│   │   ├── IAgentSystem.h
│   │   ├── ITransportSystem.h
│   │   ├── IEconomyEngine.h
│   │   ├── ISaveLoad.h
│   │   ├── IAsyncLoader.h
│   │   ├── IMemoryPool.h
│   │   ├── INetworkAdapter.h         # Empty — Phase 3
│   │   ├── IAuthority.h              # Empty — Phase 3
│   │   ├── ISpatialPartitioning.h    # Declaration only — Phase 2
│   │   ├── IRenderCommand.h
│   │   ├── IAudioCommand.h
│   │   ├── SimClock.h
│   │   ├── EventBus.h
│   │   ├── ConfigManager.h
│   │   ├── ContentRegistry.h
│   │   ├── LocaleManager.h
│   │   ├── Bootstrapper.h
│   │   ├── InputTypes.h              # GameCommand enum + struct
│   │   ├── Types.h                   # Shared types, enums, constants
│   │   └── Error.h                   # Error codes, Result<T> alias
│   │
│   ├── domain/                       # Layer 1 — RealEstateDomain (game rules here)
│   │   ├── GridSystem.h
│   │   ├── AgentSystem.h
│   │   ├── TransportSystem.h
│   │   ├── EconomyEngine.h
│   │   ├── SaveLoadSystem.h
│   │   ├── StarRatingSystem.h
│   │   └── TenantManager.h
│   │
│   ├── content/                      # Layer 2 — Content Pack
│   │   └── ContentLoader.h
│   │
│   └── renderer/                     # Layer 3 — SDL2 + ImGui
│       ├── SDLRenderer.h
│       ├── TileRenderer.h
│       ├── AgentRenderer.h
│       ├── UIRenderer.h
│       ├── InputMapper.h             # SDL_Event → GameCommand
│       └── DebugPanel.h              # Dear ImGui
│
├── src/
│   ├── core/                         # Layer 0 Core Runtime — shared services, NO game rules
│   │   ├── SimClock.cpp
│   │   ├── EventBus.cpp
│   │   ├── ConfigManager.cpp
│   │   ├── ContentRegistry.cpp
│   │   ├── LocaleManager.cpp
│   │   ├── AsyncLoader.cpp           # IAsyncLoader implementation
│   │   ├── MemoryPool.cpp            # IMemoryPool implementation
│   │   └── Bootstrapper.cpp
│   │
│   ├── domain/                       # Layer 1 — game-specific implementations
│   │   ├── GridSystem.cpp
│   │   ├── AgentSystem.cpp
│   │   ├── TransportSystem.cpp
│   │   ├── EconomyEngine.cpp
│   │   ├── SaveLoadSystem.cpp
│   │   ├── StarRatingSystem.cpp
│   │   └── TenantManager.cpp
│   │
│   ├── content/                      # Layer 2
│   │   └── ContentLoader.cpp
│   │
│   ├── renderer/                     # Layer 3
│   │   ├── SDLRenderer.cpp
│   │   ├── TileRenderer.cpp
│   │   ├── AgentRenderer.cpp
│   │   ├── UIRenderer.cpp
│   │   ├── InputMapper.cpp
│   │   └── DebugPanel.cpp
│   │
│   └── main.cpp                      # Entry point + Bootstrapper call
│
├── assets/
│   ├── config/
│   │   ├── game_config.json          # Master config (tick rate, limits, etc.)
│   │   └── balance.json              # Economy values (hot-reloadable)
│   ├── content/
│   │   └── tenants.json              # Tenant type definitions
│   ├── sprites/                      # Placeholder sprites
│   └── locale/
│       ├── ko.json
│       └── en.json
│
├── tests/
│   ├── CMakeLists.txt
│   ├── test_SimClock.cpp
│   ├── test_EventBus.cpp
│   ├── test_GridSystem.cpp
│   ├── test_AgentSystem.cpp
│   ├── test_EconomyEngine.cpp
│   ├── test_ConfigManager.cpp
│   ├── test_TransportSystem.cpp      # Elevator LOOK, boarding, capacity
│   ├── test_SaveLoadSystem.cpp       # Save → load → state equality
│   ├── test_Bootstrapper.cpp         # Init failure rollback
│   └── test_ContentRegistry.cpp      # Hot-reload apply
│
├── .github/
│   └── workflows/
│       └── build.yml
│
├── tasks/                            # Task specs (붐 → DevTeam)
│
├── CLAUDE.md                         # Implementation spec (source of truth)
├── VSE_Design_Spec.md               # This file
├── VSE_Sprint_Status.json
├── VSE_Dashboard.html
├── VSE_Overview.md
├── VSE_Architecture_Review.md
├── VSE_Session_Summary.md
├── VSE_AI_Team.md
└── VSE_CrossValidation_Prompt.md
```

---

## 2. Shared Types (include/core/Types.h)

```cpp
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

// Pixel position (render space). Origin: top-left (SDL2 convention).
// Conversion: pixelY = (config.maxFloors - 1 - floor) * config.tileSize
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

enum class ElevatorState : uint8_t {
    Idle = 0,
    MovingUp,
    MovingDown,
    DoorOpening,
    Boarding,       // Passengers entering/exiting
    DoorClosing
};

enum class StarRating : uint8_t {
    Star1 = 1,
    Star2,
    Star3,
    Star4,
    Star5
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

    // Building
    TenantPlaced = 600,
    TenantRemoved,
    StarRatingChanged,

    // Input/Command — notification AFTER command is processed (for UI feedback/logging, not for command delivery)
    GameCommandIssued = 700,

    // System
    GameSaved = 900,
    GameLoaded,
    ConfigReloaded,
};

// ── Constants ───────────────────────────────
// Role: fallback values when game_config.json fails to load.
// Runtime values always come from ConfigManager.
// Only truly compile-time-fixed values (like version numbers) use constexpr elsewhere.
namespace defaults {
    constexpr int TILE_SIZE = 32;               // px
    constexpr int MAX_FLOORS = 30;
    constexpr int MAX_NPC = 50;
    constexpr int MAX_ELEVATORS = 4;
    constexpr int ELEVATOR_SHAFTS = 1;
    constexpr double SIM_TICK_MS = 100.0;       // 10 TPS
    constexpr int TARGET_FPS = 60;
    constexpr int WINDOW_WIDTH = 1280;
    constexpr int WINDOW_HEIGHT = 720;
    constexpr int NPC_SPRITE_W = 16;            // px
    constexpr int NPC_SPRITE_H = 32;            // px
}

} // namespace vse
```

---

## 3. Error Handling (include/core/Error.h)

```cpp
#pragma once
#include <optional>
#include <string>
#include <variant>

namespace vse {

enum class ErrorCode : uint16_t {
    Ok = 0,
    InvalidCoordinate,
    FloorOutOfRange,
    TileOccupied,
    TileEmpty,
    MaxNpcReached,
    MaxFloorsReached,
    ElevatorFull,
    ElevatorNotFound,
    ElevatorShaftOccupied,
    ConfigLoadFailed,
    SaveFailed,
    LoadFailed,
    SaveVersionMismatch,
    ContentNotFound,
    InvalidTenantType,
    FloorNotBuilt,
    InitializationFailed,
};

// Result<T>: either a value or an error code
template<typename T>
using Result = std::variant<T, ErrorCode>;

// Helper to check result
template<typename T>
bool isOk(const Result<T>& r) { return std::holds_alternative<T>(r); }

template<typename T>
const T& getValue(const Result<T>& r) { return std::get<T>(r); }

template<typename T>
ErrorCode getError(const Result<T>& r) { return std::get<ErrorCode>(r); }

} // namespace vse
```

---

## 4. Input System (include/core/InputTypes.h)

> **P0-5: SDL events are never passed directly to domain. Layer 3 converts them to GameCommands.**

```cpp
// include/core/InputTypes.h
#pragma once
#include "core/Types.h"

namespace vse {

enum class GameCommandType : uint8_t {
    BuildFloor,
    PlaceTenant,
    RemoveTenant,
    PlaceElevator,
    SelectTile,
    SetGameSpeed,
    PauseToggle,
    QuickSave,
    QuickLoad,
};

struct GameCommand {
    GameCommandType type;
    TileCoord tile = {0, 0};
    TenantType tenantType = TenantType::COUNT;
    int width = 1;
    int intValue = 0;           // Speed multiplier for SetGameSpeed, etc.
    EntityId target = INVALID_ENTITY;
};

} // namespace vse
```

**Input Flow (enforced):**
```
SDL_PollEvent → InputMapper (Layer 3) → GameCommand queue → Domain systems consume
```
- `InputMapper` lives in `renderer/` (Layer 3). It converts SDL key/mouse events to `GameCommand`.
- Domain systems read from the command queue. They never see `SDL_Event`.
- This is the **only** path from user input to domain state mutation.

---

## 5. Layer 0 Interface Specifications

### 5.1 SimClock

> **P0-3: Tick-to-GameTime conversion rules are now authoritative.**

```cpp
// include/core/SimClock.h
#pragma once
#include "core/Types.h"
#include <functional>
#include <vector>

namespace vse {

class EventBus;  // forward declaration

// SimClock: tick counter + game time converter. No accumulator — that lives in Bootstrapper.
// SimClock is a pure simulation clock: advance tick, query time, emit events.
class SimClock {
public:
    explicit SimClock(EventBus& bus);

    // Called by Bootstrapper's tick loop. Advances tick by 1, emits TickAdvanced.
    void advanceTick();

    void pause();
    void resume();
    void setSpeed(int multiplier);      // 1x, 2x, 4x — Bootstrapper reads this for accumulator scaling

    // Getters
    SimTick currentTick() const;
    GameTime currentGameTime() const;   // Derived from tick via GameTime::fromTick()
    int speed() const;
    bool isPaused() const;

    // Save/Load support
    void restoreState(SimTick tick, int speed, bool paused);

private:
    EventBus& eventBus_;
    SimTick tick_ = 0;
    int speed_ = 1;
    bool paused_ = false;

    // Internal: check hour/day boundary after tick advance
    void checkTimeBoundaries(SimTick oldTick, SimTick newTick);
};

} // namespace vse
```

**SimClock Time Rules (authoritative):**
- Fixed tick: 100ms real time = 1 sim tick
- **Phase 1 mapping: 1 tick = 1 game minute** (Phase 1 hard-coded, Phase 2 configurable)
- 1440 ticks = 1 game day (24h × 60min)
- Speed multiplier (1x/2x/4x): Bootstrapper multiplies `realDeltaMs * speed` into accumulator → more ticks per second
- `tickMs` and `ticksPerGameMinute` exist in `game_config.json` for documentation only — if values differ from (100, 1), log a warning and use the hard-coded values
- `TickAdvanced` event emitted every tick
- `HourChanged` emitted when game minute crosses an hour boundary (i.e., minute was 59 → becomes 0)
- `DayChanged` emitted when game time reaches 00:00

### 5.2 EventBus

> **P0-4: Deferred-only delivery. Tick N publish → Tick N+1 flush. Immediate dispatch forbidden.**

```cpp
// include/core/EventBus.h
#pragma once
#include "core/Types.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>
#include <cstdint>

namespace vse {

struct Event {
    EventType type;
    bool replicable = false;    // Phase 3: network sync target
    EntityId source = INVALID_ENTITY;
    std::any payload;           // Type-erased data. See Event Payloads below.
};

using EventCallback = std::function<void(const Event&)>;
using SubscriptionId = uint32_t;

// Single EventBus. Phase 3 will split into Local/State channels.
class EventBus {
public:
    SubscriptionId subscribe(EventType type, EventCallback callback);
    void unsubscribe(SubscriptionId id);
    void publish(Event event);      // Always deferred. Added to queue.

    // Process queued events. Called once at start of each tick.
    // Events published during tick N are delivered at start of tick N+1.
    void flush();

    // Stats (debug)
    size_t pendingCount() const;
    size_t subscriberCount(EventType type) const;

private:
    struct Subscription {
        SubscriptionId id;
        EventCallback callback;
    };

    std::unordered_map<EventType, std::vector<Subscription>> subscribers_;
    std::vector<Event> queue_;          // Deferred queue
    std::vector<Event> processing_;     // Swap buffer during flush
    SubscriptionId nextId_ = 1;
};

} // namespace vse
```

**EventBus Delivery Rules (authoritative):**
- Phase 1 EventBus is **deferred-only**
- Events published during tick N are queued into `queue_`
- `flush()` swaps `queue_` → `processing_`, then delivers all events in `processing_`
- This means: events from tick N are consumed at the **start of tick N+1**
- **Immediate dispatch is forbidden in Phase 1** — no system may bypass the queue
- Double-buffer (queue_ / processing_) prevents infinite recursion from handlers that publish new events

**Core Event Payload Structs:**

```cpp
// Defined in EventBus.h or a separate EventPayloads.h

struct TickAdvancedPayload {
    SimTick tick;
    GameTime gameTime;
};

struct FloorBuiltPayload {
    int floor;
};

struct TenantPlacedPayload {
    EntityId entity;
    TileCoord anchor;
    TenantType type;
    int width;
};

struct ElevatorCalledPayload {
    int shaftX;
    int floor;
    ElevatorDirection preferredDir;
};

struct ElevatorArrivedPayload {
    EntityId elevatorId;
    int floor;
};

struct RentCollectedPayload {
    int totalAmount;
    int tenantCount;
    GameMinute timestamp;
};

struct StarRatingChangedPayload {
    StarRating oldRating;
    StarRating newRating;
    float avgSatisfaction;
};

struct AgentStateChangedPayload {
    EntityId agent;
    AgentState oldState;
    AgentState newState;
};

struct BalanceChangedPayload {
    int oldBalance;
    int newBalance;
    int delta;
    std::string reason;     // "rent", "construction", "maintenance"
};
```

### 5.3 ConfigManager

```cpp
// include/core/ConfigManager.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <nlohmann/json.hpp>
#include <string>

namespace vse {

// Loads game_config.json at startup. Provides typed getters.
// Hot-reload for balance.json via ContentRegistry (not ConfigManager).
class ConfigManager {
public:
    Result<bool> loadFromFile(const std::string& path);

    // Typed getters with defaults (fallback = defaults:: constants)
    int getInt(const std::string& key, int defaultVal = 0) const;
    float getFloat(const std::string& key, float defaultVal = 0.0f) const;
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;
    bool getBool(const std::string& key, bool defaultVal = false) const;

    // Direct JSON access for nested values
    const nlohmann::json& raw() const;

    // Check if key exists
    bool has(const std::string& key) const;

private:
    nlohmann::json data_;
};

} // namespace vse
```

### 5.4 IGridSystem (Interface) + GridSystem (Implementation)

> **P1-6: Grid occupancy rules for multi-tile tenants now authoritative.**

```cpp
// include/core/IGridSystem.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <optional>
#include <vector>

namespace vse {

struct TileData {
    TenantType tenantType = TenantType::COUNT;  // COUNT = empty
    EntityId tenantEntity = INVALID_ENTITY;
    bool isAnchor = false;          // true = leftmost tile of multi-tile tenant
    bool isLobby = false;
    bool isElevatorShaft = false;
    int tileWidth = 1;              // Only meaningful on anchor tile
};

class IGridSystem {
public:
    virtual ~IGridSystem() = default;

    // Grid dimensions
    virtual int maxFloors() const = 0;
    virtual int floorWidth() const = 0;

    // Build operations
    virtual Result<bool> buildFloor(int floor) = 0;
    virtual bool isFloorBuilt(int floor) const = 0;
    virtual int builtFloorCount() const = 0;

    // Tile operations
    virtual Result<bool> placeTenant(TileCoord anchor, TenantType type, int width, EntityId entity) = 0;
    virtual Result<bool> removeTenant(TileCoord anyTile) = 0;  // Finds anchor automatically
    virtual std::optional<TileData> getTile(TileCoord pos) const = 0;
    virtual bool isTileEmpty(TileCoord pos) const = 0;
    virtual bool isValidCoord(TileCoord pos) const = 0;

    // Range query
    virtual std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const = 0;

    // Elevator shaft
    virtual Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) = 0;
    virtual bool isElevatorShaft(TileCoord pos) const = 0;

    // Pathfinding support
    virtual std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const = 0;

    // Anchor lookup: given any tile of a multi-tile tenant, return the anchor tile
    virtual std::optional<TileCoord> findAnchor(TileCoord anyTile) const = 0;
};

} // namespace vse
```

**Grid Occupancy Rules (authoritative):**
- `width > 1` tenants always use the **leftmost tile as anchor**
- Non-anchor (auxiliary) tiles store the same `tenantEntity` but `isAnchor = false`
- `removeTenant(pos)` works on **any tile** of the tenant — it finds the anchor and removes the entire span
- Elevator shaft tiles cannot have tenants placed on them
- `built == false` floors reject all placement operations (return `ErrorCode::FloorNotBuilt`)
- Floor 0 is always built at game start (lobby)

```cpp
// include/domain/GridSystem.h
#pragma once
#include "core/IGridSystem.h"
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include <unordered_map>

namespace vse {

class GridSystem : public IGridSystem {
public:
    GridSystem(EventBus& bus, const ConfigManager& config);

    // All IGridSystem methods implemented
    int maxFloors() const override;
    int floorWidth() const override;
    Result<bool> buildFloor(int floor) override;
    bool isFloorBuilt(int floor) const override;
    int builtFloorCount() const override;
    Result<bool> placeTenant(TileCoord anchor, TenantType type, int width, EntityId entity) override;
    Result<bool> removeTenant(TileCoord anyTile) override;
    std::optional<TileData> getTile(TileCoord pos) const override;
    bool isTileEmpty(TileCoord pos) const override;
    bool isValidCoord(TileCoord pos) const override;
    std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const override;
    Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) override;
    bool isElevatorShaft(TileCoord pos) const override;
    std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const override;
    std::optional<TileCoord> findAnchor(TileCoord anyTile) const override;

private:
    EventBus& eventBus_;
    int maxFloors_;
    int floorWidth_;

    struct FloorData {
        std::vector<TileData> tiles;
        bool built = false;
    };
    std::unordered_map<int, FloorData> floors_;

    size_t tileIndex(int x) const;
};

} // namespace vse
```

### 5.5 IAgentSystem (Interface) + ECS Components

```cpp
// include/core/IAgentSystem.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <entt/entt.hpp>
#include <vector>
#include <optional>

namespace vse {

// ── ECS Components (data only, no logic) ────
struct PositionComponent {
    TileCoord tile;
    PixelPos pixel;             // Interpolated render position
    Direction facing = Direction::Right;
};

struct AgentComponent {
    AgentState state = AgentState::Idle;
    TenantType workplace = TenantType::COUNT;
    EntityId homeTenant = INVALID_ENTITY;
    float satisfaction = 100.0f;        // 0-100
    float moveSpeed = 1.0f;             // tiles per second
};

struct AgentScheduleComponent {
    int workStartHour = 9;
    int workEndHour = 18;
    int lunchHour = 12;
};

struct AgentPathComponent {
    std::vector<TileCoord> path;
    int currentIndex = 0;
    TileCoord destination = {0, 0};
};

struct ElevatorPassengerComponent {
    int targetFloor;
    bool waiting = true;        // true = waiting at shaft, false = inside car
};

// ── Agent System Interface ──────────────────
class IAgentSystem {
public:
    virtual ~IAgentSystem() = default;

    virtual Result<EntityId> spawnAgent(entt::registry& reg, TileCoord spawnPos, TenantType workplace, EntityId homeTenant) = 0;
    virtual void despawnAgent(entt::registry& reg, EntityId id) = 0;
    virtual int activeAgentCount() const = 0;

    // dt removed: fixed-tick = always TICK_MS. GameTime from SimClock.
    virtual void update(entt::registry& reg, const GameTime& time) = 0;

    virtual std::optional<AgentState> getState(entt::registry& reg, EntityId id) const = 0;
    virtual std::vector<EntityId> getAgentsOnFloor(entt::registry& reg, int floor) const = 0;
    virtual std::vector<EntityId> getAgentsInState(entt::registry& reg, AgentState state) const = 0;

    // For StarRatingSystem
    virtual float getAverageSatisfaction(entt::registry& reg) const = 0;
};

} // namespace vse
```

### 5.6 ITransportSystem (Elevator)

> **P1-7: Elevator finite state machine and operation rules now authoritative.**

```cpp
// include/core/ITransportSystem.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <vector>
#include <optional>

namespace vse {

struct ElevatorSnapshot {
    EntityId id;
    int currentFloor;
    float interpolatedFloor;        // For smooth rendering
    ElevatorState state;            // Idle/MovingUp/MovingDown/DoorOpening/Boarding/DoorClosing
    ElevatorDirection direction;
    int passengerCount;
    int capacity;
    std::vector<EntityId> passengers;
    std::vector<int> callQueue;
};

class ITransportSystem {
public:
    virtual ~ITransportSystem() = default;

    // Setup
    virtual Result<EntityId> createElevator(int shaftX, int bottomFloor, int topFloor, int capacity) = 0;

    // Operations
    virtual void callElevator(int shaftX, int floor, ElevatorDirection preferredDir) = 0;
    virtual Result<bool> boardPassenger(EntityId elevator, EntityId agent, int targetFloor) = 0;
    virtual void exitPassenger(EntityId elevator, EntityId agent) = 0;

    // Per-tick update — LOOK algorithm + state machine. No dt: fixed-tick.
    virtual void update(const GameTime& time) = 0;

    // Query
    virtual std::optional<ElevatorSnapshot> getElevatorState(EntityId id) const = 0;
    virtual std::vector<EntityId> getElevatorsAtFloor(int floor) const = 0;
    virtual int getWaitingCount(int floor) const = 0;
    virtual std::vector<EntityId> getAllElevators() const = 0;
};

} // namespace vse
```

**Elevator State Machine (authoritative):**

```
         ┌─────────────────────────────────┐
         │                                 │
         ▼                                 │
       Idle ──── call received ────► MovingUp/MovingDown
         ▲                                 │
         │                                 │ arrived at target floor
         │                                 ▼
         │                           DoorOpening
         │                                 │
         │                                 │ door fully open
         │                                 ▼
         │                            Boarding
         │                                 │
         │                                 │ doorOpenTicks elapsed
         │                                 ▼
         │                           DoorClosing
         │                                 │
         │           queue empty ──────────┘
         └─────── queue not empty → MovingUp/MovingDown
```

**Elevator Operation Rules (authoritative):**
- Same shaftX + same floor: duplicate hall calls merged into one
- `doorOpenTicks` (from config) = ticks the door stays open for boarding
- Capacity exceeded: remaining passengers stay in `WaitingElevator` state. The elevator does not pick them up.
- Car calls are generated when a passenger boards (from their `targetFloor`)
- LOOK: complete all stops in current direction, then reverse. No direction change mid-sweep.
- Idle: when callQueue is empty and no passengers, elevator stays at current floor

### 5.7 IEconomyEngine

```cpp
// include/core/IEconomyEngine.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <vector>
#include <string>

namespace vse {

struct IncomeRecord {
    EntityId tenantEntity;
    TenantType type;
    int amount;
    GameMinute timestamp;
};

struct ExpenseRecord {
    std::string category;       // "maintenance", "construction", "elevator"
    int amount;
    GameMinute timestamp;
};

class IEconomyEngine {
public:
    virtual ~IEconomyEngine() = default;

    virtual int getBalance() const = 0;
    virtual void addIncome(EntityId tenant, TenantType type, int amount) = 0;
    virtual void addExpense(const std::string& category, int amount) = 0;

    // Called by SimClock DayChanged event handler
    virtual void collectRent() = 0;
    virtual void payMaintenance() = 0;

    virtual void update(const GameTime& time) = 0;

    virtual int getDailyIncome() const = 0;
    virtual int getDailyExpense() const = 0;
    virtual std::vector<IncomeRecord> getRecentIncome(int count) const = 0;
    virtual std::vector<ExpenseRecord> getRecentExpenses(int count) const = 0;
};

} // namespace vse
```

### 5.8 ISaveLoad

> **P1-8: Save scope and restore order now authoritative.**

```cpp
// include/core/ISaveLoad.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <string>
#include <vector>

namespace vse {

struct SaveMetadata {
    uint32_t version = 1;       // Save format version
    std::string buildingName;
    GameTime gameTime;
    int starRating;
    int balance;
    uint64_t playtimeSeconds;
};

class ISaveLoad {
public:
    virtual ~ISaveLoad() = default;

    virtual Result<bool> save(const std::string& filepath) = 0;
    virtual Result<bool> load(const std::string& filepath) = 0;
    virtual Result<bool> quickSave() = 0;
    virtual Result<bool> quickLoad() = 0;
    virtual Result<SaveMetadata> readMetadata(const std::string& filepath) const = 0;
    virtual std::vector<std::string> listSaves() const = 0;
};

} // namespace vse
```

**Save Data Scope (Phase 1):**

| # | Data | Notes |
|---|---|---|
| 1 | SaveMetadata | version, buildingName, gameTime, starRating, balance, playtime |
| 2 | SimClock state | tick, speed, paused |
| 3 | Grid floors + tile occupancy | Per-floor built status + all TileData |
| 4 | Tenant entities + TenantComponent | All placed tenants |
| 5 | Elevator entities + ElevatorComponent | State, passengers, callQueue |
| 6 | Agent entities + all agent components | Position, state, schedule, path, satisfaction |
| 7 | Economy state | Balance, dailyIncome, dailyExpense, recent records |
| 8 | StarRating state | Current rating + avgSatisfaction |
| 9 | Current language | Locale setting |

**Load/Restore Order (authoritative):**

```
1.  Read & verify SaveMetadata (version check)
2.  ConfigManager / ContentRegistry confirm (no re-load, just verify)
3.  Clear entt::registry
4.  Restore Grid (floors + tiles)
5.  Restore Tenant entities + TenantComponent
6.  Restore Elevator entities + ElevatorComponent
7.  Restore Agent entities + all components
8.  Restore Economy state
9.  Restore StarRating state
10. Restore SimClock (tick, speed, paused)
11. Recalculate derived caches (agent paths, interpolated positions)
```

**Entity Persistence (authoritative):**
- Phase 1: **EnTT snapshot** (`entt::snapshot` / `entt::snapshot_loader`) for entity + component data
- EnTT handles entity ID remapping internally during load — no separate stable ID layer needed
- All entity cross-references (e.g., `ElevatorComponent::passengers`) are automatically remapped by EnTT
- **Non-ECS state requires custom serialization:** GridSystem internal state (`floors_`, tile occupancy), Economy state (balance, income/expense records), StarRating state — these live outside the registry and must be serialized/deserialized separately by each system
- Save pipeline: SaveMetadata + system custom data + `entt::snapshot` → MessagePack with version header
- Load pipeline: MessagePack → verify metadata → restore system state → `entt::snapshot_loader` → recalculate derived caches
- Phase 2: save format migration + optional stable ID for modding support

### 5.9 ContentRegistry

```cpp
// include/core/ContentRegistry.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <functional>

namespace vse {

class ContentRegistry {
public:
    Result<bool> loadContentPack(const std::string& directory);

    const nlohmann::json& getTenantDef(TenantType type) const;
    const nlohmann::json& getBalanceData() const;

    // Hot-reload: checks file mtime, reloads if changed
    bool checkAndReload();

    void onReload(std::function<void()> callback);

private:
    std::unordered_map<std::string, nlohmann::json> content_;
    std::string contentDir_;
    std::vector<std::function<void()>> reloadCallbacks_;
    std::unordered_map<std::string, int64_t> fileMtimes_;
};

} // namespace vse
```

### 5.10 LocaleManager

```cpp
// include/core/LocaleManager.h
#pragma once
#include "core/Error.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace vse {

class LocaleManager {
public:
    Result<bool> loadLocale(const std::string& localeDir, const std::string& lang);
    void setLanguage(const std::string& lang);
    std::string currentLanguage() const;

    std::string get(const std::string& key) const;
    std::string get(const std::string& key,
                    const std::unordered_map<std::string, std::string>& params) const;

private:
    nlohmann::json strings_;
    std::string currentLang_ = "ko";
};

} // namespace vse
```

### 5.11 IAsyncLoader

```cpp
// include/core/IAsyncLoader.h
#pragma once
#include "core/Error.h"
#include <functional>
#include <string>

namespace vse {

enum class LoadStatus : uint8_t { Pending = 0, Loading, Complete, Failed };
using LoadCallback = std::function<void(const std::string& path, LoadStatus status)>;

class IAsyncLoader {
public:
    virtual ~IAsyncLoader() = default;

    virtual void enqueue(const std::string& path, LoadCallback callback) = 0;
    virtual void processCompleted() = 0;    // Call per frame on main thread
    virtual int totalQueued() const = 0;
    virtual int totalCompleted() const = 0;
    virtual float progress() const = 0;     // 0.0 - 1.0
    virtual LoadStatus getStatus(const std::string& path) const = 0;
};

} // namespace vse
```

### 5.12 IMemoryPool

```cpp
// include/core/IMemoryPool.h
#pragma once
#include <cstddef>

namespace vse {

class IMemoryPool {
public:
    virtual ~IMemoryPool() = default;

    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr) = 0;
    virtual size_t totalAllocated() const = 0;
    virtual size_t totalCapacity() const = 0;
    virtual void reset() = 0;
};

} // namespace vse
```

### 5.13 IAudioCommand

```cpp
// include/core/IAudioCommand.h
#pragma once
#include <string>

namespace vse {

// Audio commands emitted by domain/renderer, consumed by audio subsystem.
// Phase 1: minimal — BGM + a few SFX. SDL2_mixer backend.
struct PlayBGMCmd {
    std::string trackId;        // e.g., "bgm_daytime", "bgm_night"
    float volume = 1.0f;
    bool loop = true;
};

struct PlaySFXCmd {
    std::string sfxId;          // e.g., "sfx_elevator_ding", "sfx_build"
    float volume = 1.0f;
};

struct StopBGMCmd {};

} // namespace vse
```

### 5.14 IRenderCommand

```cpp
// include/core/IRenderCommand.h
#pragma once
#include "core/Types.h"
#include <string>
#include <vector>

namespace vse {

struct RenderTileCmd {
    TileCoord pos;
    TenantType type;
    bool isAnchor;
    int spriteId;
};

struct RenderAgentCmd {
    EntityId id;
    PixelPos pos;
    Direction facing;
    AgentState state;
    int spriteFrame;
};

struct RenderElevatorCmd {
    EntityId id;
    int shaftX;
    float interpolatedY;        // Smooth movement between floors
    ElevatorState state;
    int passengerCount;
    int capacity;
};

struct RenderUICmd {
    int balance;
    GameTime time;
    int speed;
    StarRating rating;
    int npcCount;
    int maxNpc;
};

struct RenderFrame {
    std::vector<RenderTileCmd> tiles;
    std::vector<RenderAgentCmd> agents;
    std::vector<RenderElevatorCmd> elevators;
    RenderUICmd ui;
    bool isPaused;
};

} // namespace vse
```

### 5.15 Bootstrapper

> **P1-9: StarRatingSystem now included in forward declarations and owned members.**

```cpp
// include/core/Bootstrapper.h
#pragma once
#include <memory>
#include <string>

namespace vse {

class SimClock;
class EventBus;
class ConfigManager;
class ContentRegistry;
class LocaleManager;
class IGridSystem;
class IAgentSystem;
class ITransportSystem;
class IEconomyEngine;
class ISaveLoad;
class StarRatingSystem;    // Added: was missing in v1.0

// Bootstrapper: **composition root** that owns all systems and manages their lifecycle.
// It knows concrete types (GridSystem, AgentSystem, etc.) because that is the composition
// root's job. Long-term: transition to interface-based assembly (factory/DI) for testability.
class Bootstrapper {
public:
    Bootstrapper();
    ~Bootstrapper();

    bool initialize(const std::string& configPath);
    int run();          // Main loop (blocking). Returns exit code.
    void shutdown();    // Reverse init order.

private:
    // Owned systems (declaration order = init order)
    std::unique_ptr<EventBus> eventBus_;
    std::unique_ptr<ConfigManager> config_;
    std::unique_ptr<ContentRegistry> content_;
    std::unique_ptr<LocaleManager> locale_;
    std::unique_ptr<SimClock> simClock_;
    std::unique_ptr<IGridSystem> gridSystem_;
    std::unique_ptr<ITransportSystem> transportSystem_;
    std::unique_ptr<IAgentSystem> agentSystem_;
    std::unique_ptr<IEconomyEngine> economyEngine_;
    std::unique_ptr<StarRatingSystem> starRating_;  // Added
    std::unique_ptr<ISaveLoad> saveLoad_;

    // entt::registry registry_;   // In cpp
    // SDLRenderer renderer_;      // In cpp
    // InputMapper inputMapper_;   // In cpp

    // GameCommand queue (populated by InputMapper, consumed by domain)
    // std::vector<GameCommand> commandQueue_;  // In cpp

    // Tick loop state — Bootstrapper owns the accumulator, not SimClock
    double accumulator_ = 0.0;
    static constexpr int MAX_TICKS_PER_FRAME = 8;  // spiral-of-death guard

    void updateSim(double deltaMsReal);
    void render();
    void handleInput();     // SDL_PollEvent → InputMapper → commandQueue_
    void processCommands(); // drain commandQueue_ → domain system calls → clear queue
    bool running_ = false;
};

} // namespace vse
```

---

## 6. ECS Component & System Map

### 6.1 Components (all data-only)

| Component | Fields | Used By |
|---|---|---|
| `PositionComponent` | tile, pixel, facing | AgentSystem, Renderer |
| `AgentComponent` | state, workplace, homeTenant, satisfaction, moveSpeed | AgentSystem, Economy |
| `AgentScheduleComponent` | workStartHour, workEndHour, lunchHour | AgentSystem |
| `AgentPathComponent` | path[], currentIndex, destination | AgentSystem |
| `ElevatorPassengerComponent` | targetFloor, waiting | TransportSystem |
| `TenantComponent` | type, position(anchor), width, rentAmount, maintenanceCost, occupantCount, maxOccupants | EconomyEngine, GridSystem |
| `ElevatorComponent` | shaftX, bottomFloor, topFloor, capacity, currentFloor, interpolatedFloor, state, direction, callQueue, passengers | TransportSystem |
| `StarRatingComponent` (singleton) | currentRating, avgSatisfaction, totalPopulation | StarRatingSystem |
| `BuildingComponent` (singleton) | name, builtFloors, totalTenants | GridSystem |

### 6.2 Additional Component Definitions

```cpp
struct TenantComponent {
    TenantType type;
    TileCoord position;         // Anchor tile
    int width;
    int rentAmount;
    int maintenanceCost;
    int occupantCount;
    int maxOccupants;
};

struct ElevatorComponent {
    int shaftX;
    int bottomFloor;
    int topFloor;
    int capacity;
    int currentFloor;
    float interpolatedFloor;
    ElevatorState state = ElevatorState::Idle;
    ElevatorDirection direction = ElevatorDirection::Idle;
    int doorTicksRemaining = 0;     // Countdown during DoorOpening/Boarding/DoorClosing
    std::vector<int> callQueue;
    std::vector<EntityId> passengers;
};

struct StarRatingComponent {
    StarRating currentRating = StarRating::Star1;
    float avgSatisfaction = 50.0f;
    int totalPopulation = 0;
};

struct BuildingComponent {
    std::string name = "My Tower";
    int builtFloors = 1;        // Floor 0 always built
    int totalTenants = 0;
};
```

### 6.3 System Update Order (per tick)

```
1. eventBus_->flush()                — Deliver events from previous tick (tick N-1 → subscribers)
2. processCommands()                 — Drain GameCommand queue (first tick of frame only)
3. simClock_->advanceTick()          — Advance one fixed tick, emit TickAdvanced (queued for N+1)
4. agentSystem_->update(reg, time)   — NPC AI decisions, pathfinding, movement
5. transportSystem_->update(reg, time) — Elevator FSM + LOOK algorithm
6. economyEngine_->update(reg, time) — Rent/maintenance on DayChanged
7. starRatingSystem_->update(reg, time) — Recalculate satisfaction average
8. contentRegistry_->checkAndReload()   — Hot-reload (every N ticks, not every tick)
--- (outside tick loop) ---
9. render()                          — Collect RenderFrame → SDLRenderer (1x per frame)
```

> Changed from v1.0: EventBus::flush() moved to **start** of tick (was after SimClock). This ensures deferred delivery rule is respected: tick N events → flush at tick N+1 start.
> v1.3: All domain system `update()` signatures unified to `(entt::registry&, const GameTime&)`. No `float dt` parameter — fixed-tick means dt is always `TICK_MS / 1000.0`. Systems that need registry get it from Bootstrapper. GameTime from SimClock.

---

## 7. Module Dependency Map

```
                    ┌──────────────────┐
                    │   Bootstrapper   │
                    │  (owns everything)│
                    └────────┬─────────┘
                             │ creates & initializes
           ┌─────────┬──────┼──────┬───────────┬──────────┐
           ▼         ▼      ▼      ▼           ▼          ▼
      EventBus   Config  Content  Locale    SimClock    SaveLoad
           │         │      │                  │
           │         │      │                  │
    ┌──────┼─────────┼──────┼──────────────────┤
    ▼      ▼         ▼      ▼                  ▼
 GridSystem      AgentSystem        TransportSystem
    │                │                     │
    │                │                     │
    └────────┬───────┴─────────────────────┘
             ▼
        EconomyEngine
             │
             ▼
        StarRatingSystem
             │
             ▼
     ┌───────────────┐
     │  RenderFrame   │ ← Collected from all systems
     └───────┬───────┘
             ▼
     ┌───────────────┐       ┌──────────────┐
     │  SDLRenderer   │ ◄──── │ InputMapper  │
     │  Layer 3       │       │ GameCommand  │
     └───────────────┘       └──────────────┘
```

### Dependency Rules (enforced):
- **EventBus**: depended on by ALL systems. No deps itself.
- **ConfigManager**: depended on by GridSystem, AgentSystem, Transport, Economy. No deps.
- **SimClock** → EventBus only
- **GridSystem** → EventBus, ConfigManager
- **AgentSystem** → EventBus, ConfigManager, GridSystem (read-only), TransportSystem (call elevator)
- **TransportSystem** → EventBus, ConfigManager
- **EconomyEngine** → EventBus, ConfigManager, GridSystem (read tenant info)
- **StarRatingSystem** → EventBus, AgentSystem (read satisfaction)
- **InputMapper** → SDL2 only. Produces GameCommand. No domain access.
- **SDLRenderer** → RenderFrame only. No domain system access.

---

## 8. Initialization Order (Bootstrapper)

```
 1. EventBus          — No deps
 2. ConfigManager     — Load game_config.json
 3. ContentRegistry   — Load content packs (tenants.json, balance.json)
 4. LocaleManager     — Load locale/ko.json
 5. SimClock          — Needs EventBus
 6. GridSystem        — Needs EventBus, ConfigManager
 7. TransportSystem   — Needs EventBus, ConfigManager
 8. AgentSystem       — Needs EventBus, ConfigManager, GridSystem, TransportSystem
 9. EconomyEngine     — Needs EventBus, ConfigManager, GridSystem
10. StarRatingSystem  — Needs EventBus, AgentSystem
11. SaveLoad          — Needs all above (serializes everything)
12. SDLRenderer       — Needs ConfigManager (window size). SDL_Init here.
13. InputMapper       — Needs SDLRenderer context
14. DebugPanel        — Needs SDLRenderer (ImGui context)

Shutdown: reverse order (14 → 1)

Initialize failure at any step: log error, shutdown already-initialized systems in reverse, return false.
```

---

## 9. Data Flow Diagrams

### 9.1 Main Loop — True Fixed-Tick Loop (authoritative)

```
┌─ Frame Start ──────────────────────────────────────────────┐
│                                                             │
│  1. handleInput()                                           │
│     └─ SDL_PollEvent → InputMapper → commandQueue_          │
│                                                             │
│  2. updateSim(realDeltaMs)                                 │
│     accumulator_ += realDeltaMs * simClock_->speed()        │
│     ticksThisFrame = 0                                      │
│                                                             │
│     while (accumulator_ >= TICK_MS                          │
│            && ticksThisFrame < MAX_TICKS_PER_FRAME          │
│            && !simClock_->isPaused()):                       │
│       ├─ accumulator_ -= TICK_MS                            │
│       ├─ ticksThisFrame++                                   │
│       ├─ EventBus::flush()         [deliver tick N-1 events]│
│       ├─ simClock_->advanceTick()  [tick N, queue→N+1]     │
│       ├─ if (ticksThisFrame == 1):                          │
│       │     processCommands()      [drain commandQueue_]    │
│       ├─ AgentSystem::update()                              │
│       ├─ TransportSystem::update()                          │
│       ├─ EconomyEngine::update()                            │
│       ├─ StarRatingSystem::update()                         │
│       └─ ContentRegistry::checkAndReload() [every N ticks]  │
│                                                             │
│  3. render()                      [1x per frame, outside tick loop]│
│     ├─ Collect RenderFrame from all systems                 │
│     ├─ SDLRenderer::render(frame)                          │
│     │   ├─ TileRenderer::draw(frame.tiles)                 │
│     │   ├─ AgentRenderer::draw(frame.agents)               │
│     │   ├─ UIRenderer::draw(frame.ui)                      │
│     │   └─ DebugPanel::draw() [if debug mode]              │
│     └─ SDL_RenderPresent()                                  │
│                                                             │
└─ Frame End ────────────────────────────────────────────────┘
```

**Fixed-tick rules:**
- System update runs per **tick**, not per frame
- **Speed mechanism:** `accumulator_ += realDeltaMs * speed`. Speed 2x → accumulator fills 2× faster → ~20 ticks/sec. Speed 4x → ~40 ticks/sec.
- **MAX_TICKS_PER_FRAME:** spiral-of-death guard. Value = `speed * 2` (e.g., 4x → cap 8). Excess accumulator carries over to next frame.
- **Accumulator lives in Bootstrapper** (not SimClock). SimClock is a pure tick counter.
- **processCommands():** drains the entire commandQueue_ and clears it. Runs only on the first tick of each frame to prevent duplicate command execution.
- **Pause:** `simClock_->isPaused()` skips the while loop entirely. Accumulator does NOT grow while paused.
- `render()` runs exactly once per frame regardless of how many ticks were processed
- When `accumulator_ < TICK_MS`, no sim update runs — only render with interpolated positions

### 9.2 NPC Daily Cycle

```
Spawn (morning, when workStartHour - commute time)
  │
  ▼
Idle at home floor
  │ workStartHour approaching → find path
  ▼
Moving → path to office (may use elevator)
  │ arrived
  ▼
Working (state = Working)
  │ lunchHour reached
  ▼
Moving → path to commercial floor or lobby
  │ arrived
  ▼
Resting (state = Resting, ~1 game hour)
  │ lunch over
  ▼
Moving → back to office
  │ arrived
  ▼
Working
  │ workEndHour reached
  ▼
Moving → path to home / exit
  │ arrived
  ▼
Despawn (evening)

Special: if satisfaction < leaveThreshold → state = Leaving → despawn immediately
```

### 9.3 Elevator Flow (LOOK + State Machine)

```
Agent at floor X, needs floor Y
  │
  ▼
AgentSystem: callElevator(shaftX, floorX, direction)
  │
  ▼
TransportSystem: merge call into callQueue (deduplicate)
  │
  ▼
Elevator FSM: Idle → MovingUp/MovingDown
  │ arrived at floor X
  ▼
DoorOpening (animation ticks)
  │
  ▼
Boarding: passengers exit (targetFloor == currentFloor) then board (if capacity allows)
  │ Agent boards → AgentState = InElevator
  │ Car call added for floor Y
  ▼
DoorClosing (animation ticks)
  │
  ▼
callQueue empty? → Idle
callQueue not empty? → MovingUp/MovingDown (LOOK: finish current direction first)
  │ arrived at floor Y
  ▼
DoorOpening → Agent exits → AgentState = Moving → DoorClosing
```

---

## 10. JSON Config Schemas

### 10.1 game_config.json

```json
{
    "version": 1,
    "window": {
        "width": 1280,
        "height": 720,
        "title": "Tower Tycoon"
    },
    "simulation": {
        "tickMs": 100,
        "ticksPerGameMinute": 1,
        "maxFloors": 30,
        "maxNpc": 50,
        "maxElevators": 4,
        "startingBalance": 50000
    },
    "grid": {
        "tileSize": 32,
        "floorWidth": 40,
        "lobbyFloor": 0
    },
    "elevator": {
        "speed": 2.0,
        "capacity": 8,
        "doorOpenTicks": 3
    },
    "render": {
        "targetFps": 60,
        "enableDebugPanel": true
    }
}
```

### 10.2 balance.json (hot-reloadable)

```json
{
    "tenants": {
        "office": {
            "rent": 500,
            "maintenance": 100,
            "maxOccupants": 6,
            "width": 4,
            "satisfactionDecayRate": 0.1,
            "buildCost": 5000
        },
        "residential": {
            "rent": 300,
            "maintenance": 50,
            "maxOccupants": 3,
            "width": 3,
            "satisfactionDecayRate": 0.05,
            "buildCost": 3000
        },
        "commercial": {
            "rent": 800,
            "maintenance": 200,
            "maxOccupants": 0,
            "width": 5,
            "satisfactionDecayRate": 0.15,
            "buildCost": 8000
        }
    },
    "npc": {
        "baseMoveSpeed": 1.0,
        "satisfactionGainWork": 0.5,
        "satisfactionLossWait": -2.0,
        "satisfactionLossNoElevator": -5.0,
        "leaveThreshold": 20.0
    },
    "starRating": {
        "thresholds": [0, 30, 50, 70, 90]
    },
    "economy": {
        "floorBuildCost": 10000,
        "elevatorBuildCost": 15000,
        "elevatorMaintenancePerDay": 200
    }
}
```

### 10.3 tenants.json (Content Pack)

```json
{
    "tenantTypes": [
        {
            "type": "office",
            "displayName": { "ko": "사무실", "en": "Office" },
            "sprite": "office_32x32.png",
            "spriteFrames": 1,
            "description": { "ko": "기업 입주 사무실", "en": "Corporate office space" }
        },
        {
            "type": "residential",
            "displayName": { "ko": "주거", "en": "Residential" },
            "sprite": "residential_32x32.png",
            "spriteFrames": 1,
            "description": { "ko": "주거 공간", "en": "Residential unit" }
        },
        {
            "type": "commercial",
            "displayName": { "ko": "상업", "en": "Commercial" },
            "sprite": "commercial_32x32.png",
            "spriteFrames": 2,
            "description": { "ko": "상업 시설 (식당, 상점)", "en": "Commercial (restaurant, shop)" }
        }
    ]
}
```

---

## 11. Phase 3 Stub Interfaces (Empty)

```cpp
// include/core/INetworkAdapter.h
#pragma once
namespace vse {
// [PHASE-3] Network adapter for authoritative server model.
class INetworkAdapter {
public:
    virtual ~INetworkAdapter() = default;
};
} // namespace vse

// include/core/IAuthority.h
#pragma once
namespace vse {
// [PHASE-3] Authority for server-side state validation.
class IAuthority {
public:
    virtual ~IAuthority() = default;
};
} // namespace vse

// include/core/ISpatialPartitioning.h
#pragma once
namespace vse {
// [PHASE-2] Spatial partitioning for NPC queries.
// Phase 1: brute-force (50 NPCs). Implement after perf profiling.
class ISpatialPartitioning {
public:
    virtual ~ISpatialPartitioning() = default;
};
} // namespace vse
```

---

## 12. Layer Boundary Enforcement Checklist

| Rule | How to Verify |
|---|---|
| Layer 3 has no game logic | `grep -rn "satisfaction\|balance\|rent\|starRating" src/renderer/` returns nothing |
| Core Runtime has no game rules | `grep -rn "satisfaction\|rent\|starRating" src/core/` returns nothing |
| Layer 0 Core API has no concrete impl | All `include/core/I*.h` are pure virtual or POD structs |
| Layer 1 has no SDL2 calls | `grep -rn "SDL_\|#include.*SDL" src/domain/ include/domain/` returns nothing |
| Domain produces RenderFrame only | Domain systems return data structs, never call render functions |
| Input goes through GameCommand | `grep -rn "SDL_Event\|SDL_Poll" src/domain/ src/core/` returns nothing |
| Domain never reads JSON directly | `grep -rn "nlohmann::json\|json::" src/domain/ include/domain/` returns nothing |

---

## 13. Defaults vs Config Policy

| Category | Source | Example |
|---|---|---|
| **Compile-time fallback** | `defaults::` namespace in `Types.h` | `MAX_FLOORS = 30` — used if JSON load fails |
| **Runtime values** | `game_config.json` via ConfigManager | `config.getInt("simulation.maxFloors", defaults::MAX_FLOORS)` |
| **Hot-reloadable tuning** | `balance.json` via ContentRegistry | Rent, satisfaction rates — change without restart |
| **Content definitions** | `tenants.json` via ContentRegistry | Tenant types, sprites |
| **Immutable (Phase 1)** | Hard-coded in SimClock | `tickMs=100`, `ticksPerGameMinute=1` — config mismatch → warn + use hard-coded |

**Rules:**
- Code always reads from ConfigManager/ContentRegistry. `defaults::` constants are **only** used as fallback parameters in `getInt(key, default)` calls. No direct use of `defaults::` in game logic.
- **Domain layer must not read JSON directly.** JSON loading is handled only by ConfigManager and ContentRegistry (approved runtime/content services).
- Content hot-reload frequency: `Bootstrapper` calls `ContentRegistry::checkAndReload()` every 600 ticks (~60 seconds at 10 TPS). This interval is a `defaults::` constant, not from config.

---

## Change Log

| Date | Version | Change |
|---|---|---|
| 2026-03-24 | 1.0 | Initial design spec. All Layer 0 interfaces, ECS components, dependency map, init order, JSON schemas, data flow. |
| 2026-03-24 | 1.1 | Cross-review 11 items applied: (P0) Layer 0 split clarified, EntityId=entt::entity, SimClock tick-time rules, EventBus deferred-only with payload structs, GameCommand input flow; (P1) Grid anchor/occupancy rules, Elevator FSM 6-state, SaveLoad scope+restore order, StarRatingSystem/AsyncLoader/MemoryPool consistency fixes, IAudioCommand spec; (P2) defaults-vs-config policy, test coverage expanded. System update order corrected (flush first). |
| 2026-03-24 | 1.2 | Final review 6 items: (1) true fixed-tick loop with accumulator + maxTicksPerFrame spiral-of-death guard, (2) CLAUDE.md synced to Core API/Runtime split — now v1.2, (3) EnTT snapshot-based save/restore confirmed, (4) Bootstrapper = composition root documented, (5) tickMs/ticksPerGameMinute immutable in Phase 1 — config mismatch = warn + hard-code, (6) JSON reader rule tightened, hot-reload interval = 600 ticks constant. |
| 2026-03-24 | 1.3 | Self-review 9 fixes: **[Critical]** (C-1) speed multiplier now `accumulator += dt * speed` instead of tick cap, (C-2) accumulator ownership moved from SimClock to Bootstrapper — SimClock is pure tick counter; **[Important]** (I-1) Core API header rule clarified (I*.h = pure virtual, others = runtime declarations), (I-2) processCommands() drain-once-per-frame rule, (I-3) system update() signatures unified to `(registry&, GameTime&)` — no dt param, (I-4) SaveLoad: non-ECS state (Grid floors_, Economy balance) requires custom serialization alongside EnTT snapshot; **[Minor]** PixelPos config ref, GameCommandIssued = post-process notification, TileCoord std::hash added. |

---
*This document is maintained by 붐 (PM). Changes require Human approval.*
*Implementation must follow CLAUDE.md constraints. This doc provides the HOW.*
