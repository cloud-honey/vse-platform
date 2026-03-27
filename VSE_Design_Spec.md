# VSE Design Specification — Phase 1
> Version: 3.0 | Author: 붐 (PM) | Date: 2026-03-27
> Source of truth: `CLAUDE.md` v1.3 | This document details HOW to implement what CLAUDE.md defines.
> CLAUDE.md = WHAT (rules, constraints) | This doc = HOW (signatures, data flow, init order)
> v1.1: Cross-review 11 items applied (Layer 0 split, EntityId=entt, tick-time rules, deferred EventBus, GameCommand, grid occupancy, elevator FSM, save/load scope, consistency fixes, defaults policy, test coverage)
> v1.2: Final review 6 items — true fixed-tick loop, CLAUDE.md sync, EnTT snapshot save/restore, Bootstrapper=composition root, config immutable ticks, document consistency
> v1.5: Sprint 2 implementation sync (Bootstrapper API, SaveMetadata balance int64_t, StarRatingChangedPayload, manual JSON+MessagePack save strategy)
> v2.0: Sprint 3 implementation sync + Sprint 4 planned systems
> v2.5: Sprint 4 implementation sync + Sprint 5 planned systems
> v3.0: Sprint 5 implementation sync + Sprint 6 planned systems (visual + audio)

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
│   │   └── game_config.json          # Master config (tick rate, limits, etc.)
│   ├── data/
│   │   └── balance.json              # Economy values (hot-reloadable, tenants+economy schema)
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

// Pixel position — world-space coordinates (bottom-left origin, Y increases upward).
// Sprint 1 확정 규칙 (2026-03-25):
//   pixelX = tile.x * config.tileSize
//   pixelY = tile.floor * config.tileSize   (NOT SDL2 screen space)
// Camera::worldToScreenX/Y() converts to SDL2 screen space (top-left, Y↓).
// PositionComponent.pixel = NPC foot (발바닥) position in world space.
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
    InsufficientFunds,

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

// ── Event Payloads ──────────────────────────
// Shared payload structs used with EventBus.
// TASK-02-009: StarRatingChangedPayload moved from StarRatingSystem.h → Types.h
//              (reduces coupling: renderers/listeners no longer need to include StarRatingSystem.h)
struct StarRatingChangedPayload {
    int oldRating;   // Previous star rating (1-5)
    int newRating;   // New star rating (1-5)
};

// Sprint 4: Insufficient funds notification
struct InsufficientFundsPayload {
    std::string action;      // "buildFloor", "placeTenant", "placeElevator"
    int64_t required;        // Cents needed
    int64_t available;       // Current balance
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

// NOTE: Actual implementation (Error.h) uses a simplified enum.
// This spec lists the original design; implementation uses:
//   None=0, FileNotFound, FileReadError, JsonParseError,
//   InvalidArgument, OutOfRange, SystemError, FloorNotBuilt
// Spec values below are the original design-time names.
// When in doubt, Error.h is authoritative.
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
    AgentState state            = AgentState::Idle;
    EntityId   homeTenant       = INVALID_ENTITY;   // 거주지 테넌트
    EntityId   workplaceTenant  = INVALID_ENTITY;   // 직장 테넌트 (목적지 특정용)
    float      satisfaction     = 100.0f;           // 0-100
    float      moveSpeed        = 1.0f;             // tiles/sec
    float      stress           = 0.0f;             // 0-100, increases on wait/no-elevator
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

    // homeTenantId: resident's home tenant entity. workplaceId: assigned workplace tenant entity.
    // spawnPos: optional override — if not provided, defaults to homeTenant position.
    virtual Result<EntityId> spawnAgent(entt::registry& reg, EntityId homeTenantId, EntityId workplaceId, std::optional<TileCoord> spawnPos = std::nullopt) = 0;
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

**Implementation guide — internal separation within TransportSystem:**
- `ElevatorCarFSM`: per-elevator state machine handling door open/close, dwell time, boarding/alighting, movement state transitions. Each car owns its own FSM instance.
- `LookScheduler`: collects hall calls + car calls, determines next stop floor, enforces same-direction-first rule, merges duplicate calls. Shared across all elevators in a shaft group.
- `TransportSystem::update()` iterates each elevator: (1) LookScheduler picks next target, (2) ElevatorCarFSM advances state, (3) emit events (ElevatorArrived, PassengerBoarded, etc.)
- This is an **internal design guideline**, not a public API boundary — both classes live inside `src/domain/TransportSystem.cpp`.

### 5.7 IEconomyEngine

> **TASK-02-003 업데이트 (2026-03-25):**
> - 화폐 단위: `int` → `int64_t Cents` (오버플로우 방지)
> - `GameMinute` timestamp → `GameTime` (통일성)
> - `collectRent` / `payMaintenance`: 파라미터 없음 → `const IGridSystem& grid, const GameTime& time` (외부 의존성 명시)
> - `addIncome` / `addExpense`: `const GameTime& time` 파라미터 추가
> - `ExpenseRecord::category` 예시: `"maintenance", "construction", "elevator"` → `"rent", "maintenance", "elevator"`

```cpp
// include/core/IEconomyEngine.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include "core/IGridSystem.h"
#include <vector>
#include <string>

namespace vse {

struct IncomeRecord {
    EntityId   tenantEntity;
    TenantType type;
    int64_t    amount;    // Cents
    GameTime   timestamp;
};

struct ExpenseRecord {
    std::string category; // "rent", "maintenance", "elevator"
    int64_t     amount;   // Cents
    GameTime    timestamp;
};

class IEconomyEngine {
public:
    virtual ~IEconomyEngine() = default;

    virtual int64_t getBalance() const = 0;
    virtual void addIncome(EntityId tenant, TenantType type, int64_t amount, const GameTime& time) = 0;
    virtual void addExpense(const std::string& category, int64_t amount, const GameTime& time) = 0;

    // Called once per game day (DayChanged event)
    virtual void collectRent(const IGridSystem& grid, const GameTime& time) = 0;
    virtual void payMaintenance(const IGridSystem& grid, const GameTime& time) = 0;

    // Called every tick by Bootstrapper
    virtual void update(const GameTime& time) = 0;

    virtual int64_t getDailyIncome()  const = 0;
    virtual int64_t getDailyExpense() const = 0;
    virtual std::vector<IncomeRecord>  getRecentIncome(int count)   const = 0;
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
    uint32_t    version = 1;       // Save format version
    std::string buildingName;
    GameTime    gameTime;
    int         starRating = 1;
    int64_t     balance    = 0;    // Cents (TASK-02-003: int → int64_t)
    uint64_t    playtimeSeconds = 0;
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
- Phase 1: **Manual JSON serialization** with two-pass EntityId remap table. Originally planned as `entt::snapshot` / `entt::snapshot_loader`, but changed to manual approach because: (a) only 5 ECS component types in Phase 1, (b) `entt::snapshot` requires a custom archive adapter for MessagePack output — added complexity without benefit at this scale, (c) manual approach gives full control over cross-reference remapping (Grid tenantEntity, Transport passengers). Phase 2+ may switch to `entt::snapshot` if component count grows significantly.
- Entity cross-references (e.g., `ElevatorCar::passengers`, `AgentComponent::homeTenant`, `AgentComponent::workplaceTenant`, `Grid TileData::tenantEntity`) are remapped via a `unordered_map<uint32_t, EntityId>` remap table built during entity restoration. SaveLoad tests explicitly verify these references survive a round-trip.
- **Non-ECS state requires custom serialization:** GridSystem internal state (`floors_`, tile occupancy), Economy state (balance, income/expense records), StarRating state — these live outside the registry and must be serialized/deserialized separately by each system via `exportState()`/`importState()` methods.
- **Restore sequence:** (1) Read & verify SaveMetadata (version check), (2) Clear registry + agent tracking, (3) Restore Grid (floors + tiles), (4) Restore ECS entities (two-pass: create + remap), (5) Remap Grid tenantEntity references, (6) Restore Economy, (7) Restore StarRating, (8) Restore Transport + remap passenger IDs, (9) Restore SimClock (silent), (10) Recalculate derived caches.
- Save pipeline: SaveMetadata + system state + entity data → JSON → `nlohmann::json::to_msgpack()` → binary file
- **SaveLoad test requirements (Phase 1):** passenger EntityId round-trip, homeTenant/workplace reference validity, Grid tile occupancy consistency after load, Economy balance equality
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

> **TASK-02-001 업데이트 (2026-03-25):**
> - API: `initialize(configPath)` → `init()` / `run()` / `shutdown()` (SDL 포함 풀 초기화)
> - 헤드리스 테스트용 `initDomainOnly(configPath)` 추가
> - 시스템 소유 방식: `std::unique_ptr<I*>` → 직접 멤버 (`ConfigManager config_`, `EventBus eventBus_`, `SimClock simClock_` 등)
> - `SimClock`은 직접 멤버로 값 소유 (`simClock_{eventBus_}`로 초기화) — 시간 진행의 단일 소유자
> - `#ifdef VSE_TESTING` 가드: 테스트 접근자 프로덕션 빌드에서 제거
> - `setupInitialScene()` 헬퍼 추출: `init()` / `initDomainOnly()` 공용 씬 설정 중복 제거
> - `processCommands()` 주석: frame당 1회 drain, tick 루프 밖 호출 = "첫 tick drain" 계약과 동일

```cpp
// include/core/Bootstrapper.h — TASK-02-001 실제 구현 반영
#pragma once
#include "core/EventBus.h"
#include "core/ConfigManager.h"
#include "core/SimClock.h"
#include "core/ContentRegistry.h"
#include "domain/GridSystem.h"
#include "domain/AgentSystem.h"
#include "domain/TransportSystem.h"
#include "domain/EconomyEngine.h"
#include "domain/StarRatingSystem.h"
#include "domain/TenantSystem.h" // Added TenantSystem
#include "renderer/SDLRenderer.h"
#include "renderer/Camera.h"
#include "renderer/InputMapper.h"
#include "renderer/RenderFrameCollector.h"
#include "core/InputTypes.h"
#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace vse {

class Bootstrapper {
public:
    Bootstrapper() = default;

    bool init();           // 모든 시스템 초기화 (SDL 포함). false 시 main() 즉시 종료.
    void run();            // 메인 루프 (SDL 윈도우 폐쇄 또는 Quit 커맨드까지)
    void shutdown();       // 정리 (SDL 종료)

    // SDL 없이 Domain만 조립 (headless 단위 테스트용)
    // configPath: "assets/"로 시작하면 VSE_PROJECT_ROOT 기준 절대경로로 변환
    bool initDomainOnly(const std::string& configPath = "assets/config/game_config.json");

#ifdef VSE_TESTING
    // 테스트 전용 접근자 (VSE_TESTING 빌드에서만 노출, 프로덕션 빌드 심볼 생성 안 됨)
    bool testGetPaused()  const { return simClock_.isPaused(); }
    int  testGetSpeed()   const { return simClock_.speed(); }
    bool testGetRunning() const { return running_; }
    void testProcessCommands(const std::vector<GameCommand>& cmds) {
        processCommands(cmds, running_);
    }
#endif

private:
    // frame당 1회 drain. tick 루프 밖에서 호출 = "첫 tick drain" 계약 동일하게 충족.
    void processCommands(const std::vector<GameCommand>& cmds, bool& running);
    void fillDebugInfo(RenderFrame& frame, int realDeltaMs);
    void setupInitialScene();  // init() / initDomainOnly() 공용 — 프로토타입 씬 구성

    // 실행 상태
    bool running_     = false;
    int  accumulator_ = 0;     // Bootstrapper 소유 (SimClock에 없음)

    // Core Runtime (선언 순서 = 초기화 순서)
    ConfigManager   config_;
    ContentRegistry content_; // Added content_
    EventBus        eventBus_;
    SimClock        simClock_{eventBus_};  // 시간 진행의 단일 소유자
    entt::registry  registry_;

    // Domain (Layer 1)
    std::unique_ptr<GridSystem>      grid_;
    std::unique_ptr<AgentSystem>     agents_;
    std::unique_ptr<TransportSystem> transport_;
    std::unique_ptr<EconomyEngine>   economy_;    // Added economy_
    std::unique_ptr<StarRatingSystem> starRating_; // Added starRating_
    std::unique_ptr<TenantSystem>    tenantSystem_; // Added tenantSystem_
    std::unique_ptr<GameOverSystem>  gameOver_;     // Added Sprint 4
    GameStateManager gameState_{eventBus_};        // Added Sprint 4

    // Renderer (Layer 3)
    SDLRenderer   sdlRenderer_;
    Camera        camera_;
    InputMapper   inputMapper_;
    std::unique_ptr<RenderFrameCollector> collector_;

    // 설정 캐시 (init() 시 ConfigManager에서 읽음)
    int   windowW_    = 0;
    int   windowH_    = 0;
    int   tileSizePx_ = 0;
    int   tickMs_     = 100;
    float zoomMin_    = 0.25f;
    float zoomMax_    = 4.0f;
    float panSpeed_   = 8.0f;

    bool drawGrid_  = true;
    bool drawDebug_ = true;
};

} // namespace vse
```

**주요 설계 결정:**
- `EconomyEngine`, `StarRatingSystem`, `SaveLoadSystem`, `ContentRegistry`는 `include/domain/` 또는 `src/domain/`에 구현체 존재. `Bootstrapper`가 직접 `unique_ptr`로 소유 (Phase 1에서는 아직 `Bootstrapper` 멤버 미포함 — Sprint 3에서 통합 예정).
- `MAX_TICKS_PER_FRAME = 8` spiral-of-death guard: `Bootstrapper::run()` 내 accumulator 루프에서 적용.

### 5.16 NPC Stress System

> **Sprint 3 Implementation (2026-03-26):**
> - AgentComponent now includes `float stress` field (0.0–100.0)
> - Stress increases when NPC waits for elevator or experiences delays
> - Satisfaction decreases when stress > 50.0
> - Stress serialized via SaveLoadSystem
> - `AgentSatisfactionChanged` event fires when satisfaction changes
> - Leave threshold: satisfaction < 10.0 (10%)

**Stress Mechanics:**
- Stress accumulates at configurable rate when NPC is in `WaitingElevator` state
- Additional stress penalty when elevator wait exceeds timeout threshold
- Stress decays gradually when NPC is in `Working` or `Resting` states
- When stress > 50.0, satisfaction decreases proportionally
- High stress (> 80.0) triggers faster satisfaction decay

**Implementation Details:**
- Stress field added to `AgentComponent` in `IAgentSystem.h`
- `AgentSystem::updateSatisfactionAndStress()` handles stress accumulation/decay
- Stress values serialized in save/load via ECS component persistence
- `AgentSatisfactionChanged` event payload includes old/new satisfaction values

### 5.17 TenantSystem

> **Sprint 3 Implementation (2026-03-26):**
> - Manages tenant lifecycle: placement, removal, rent collection, maintenance payments
> - TenantComponent fields: anchorTile, rentPerDay, maintenanceCostPerDay, evictionCountdown
> - `TenantSystem::onDayChanged()` → calls `EconomyEngine.collectRent()` and `EconomyEngine.payMaintenance()`
> - Eviction reset: if satisfaction recovers before eviction, countdown resets
> - Serialized via SaveLoadSystem

**TenantComponent Structure:**
```cpp
struct TenantComponent {
    TenantType  type            = TenantType::COUNT;
    TileCoord   anchorTile      = {0, 0};
    int         width           = 1;
    int64_t     rentPerDay      = 0;      // Cents
    int64_t     maintenanceCost = 0;      // Cents per day
    int         maxOccupants    = 0;
    int         occupantCount   = 0;      // Current occupant NPC count
    int64_t     buildCost       = 0;      // Cents (one-time)
    float       satisfactionDecayRate = 0.0f; // Per-tick rate for NPC satisfaction
    bool        isEvicted       = false;
    int         evictionCountdown = 0;    // Ticks remaining before despawn (0 = not evicting)
};
```

**Tenant Lifecycle:**
1. **Placement**: `TenantSystem::placeTenant()` creates entity with TenantComponent, calls `GridSystem::placeTenant()`
2. **Daily Operations**: `onDayChanged()` collects rent from all tenants, pays maintenance costs
3. **Eviction Process**: When average NPC satisfaction in tenant < threshold, eviction countdown starts
4. **Recovery**: If satisfaction improves above threshold before countdown reaches 0, eviction resets
5. **Removal**: Countdown reaches 0 → tenant marked evicted, removed from grid, entity destroyed

**Dependencies:**
- `IGridSystem`: Tenant placement/removal on grid
- `IEconomyEngine`: Rent collection and maintenance payments
- `EventBus`: TenantPlaced/TenantRemoved events
- `ContentRegistry`: Tenant attributes from balance.json

### 5.18 Economy Loop

> **Sprint 4 Implementation (2026-03-26, TASK-04-001):**
> - `TenantSystem::onDayChanged()` → calls `EconomyEngine.collectRent()` / `EconomyEngine.payMaintenance()` per tenant
> - `buildFloor()` deducts cost from balance; rejects with `ErrorCode::InsufficientFunds` if balance < cost
> - `placeTenant()` deducts `buildCost` (from balance.json) at placement time
> - `InsufficientFunds` event payload: `{action, requiredAmount, availableAmount}`
> - Elevator maintenance paid daily via `EconomyEngine::payMaintenance()`

**Daily Economy Cycle:**
- **Midnight (day changes)**: `TenantSystem::onDayChanged()` → rent collection + maintenance per tenant
- **Real-time**: Construction costs deducted immediately (`buildFloor`, `placeTenant`)
- **Elevator**: Fixed maintenance cost per day from `balance.json` `economy.elevatorMaintenancePerDay`

**Funds Validation:**
- `EconomyEngine::buildFloor()` / `placeTenant()` check balance before executing
- Insufficient funds → `InsufficientFunds` event (EventType 800), action blocked
- `InsufficientFundsPayload.action` carries string description of blocked action

**HUD Integration:**
- `HUDPanel` (ImGui) displays balance with thousand-separator formatting (e.g., ₩1,234,567)
- Balance color: green when positive, red when negative
- DailySettlement event triggers ImGui toast notification

### 5.19 Stair Movement System

> **Sprint 4 Implementation (2026-03-26, TASK-04-002):**
> - `AgentSystem` stair logic: `|targetFloor - currentFloor| <= 4` → use stairs directly
> - Stair speed: 2 ticks per floor (`stairTicksRemaining = abs(delta) * 2`)
> - State: `AgentState::UsingStairs` — dedicated enum value (not `Walking`) for clear FSM separation; rendered orange in `AgentRenderer`
> - Elevator wait timeout fallback: if NPC has been `WaitingElevator` > threshold ticks AND floor distance ≤ 4 → switch to `UsingStairs`
> - Direction agnostic: stairs used for both ascent and descent ≤4 floors

**Stair Mechanics:**
- **Distance Threshold**: `|delta| <= 4` → stairs preferred; `>= 5` → elevator required
- **Speed**: 2 ticks/floor (vs elevator: LOOK algorithm, variable)
- **Intermediate floors**: NPC floor advances 1 per 2 ticks until `stairTargetFloor` reached
- **State guard**: `UsingStairs` with `stairTargetFloor = -1` or `stairTicksRemaining = 0` → reset to `Idle` (defensive)

**Fallback Logic:**
- 5+ floor NPC calls elevator; if wait > `elevatorWaitTimeout` ticks AND distance ≤ 4 → override to stairs
- Timeout threshold configurable via `balance.json` (default from `AgentSystem` constant)
- Prevents NPCs stuck on busy elevators for short-distance trips

**Rendering:**
- `UsingStairs` NPC color: orange rectangle in `AgentRenderer`
- Position interpolates toward target floor during movement

### 5.20 Periodic Settlement System

> **Sprint 4 Implementation (2026-03-26, TASK-04-003):**
> - `EconomyEngine::onDayChanged()` fires `DailySettlement` event every day
> - Weekly report: `DailySettlement` + `WeeklyReport` event every 7 days
> - Quarterly settlement: every 90 days — tax deduction + `StarRatingReEvalRequested` event
> - Tax formula: `tax = balance * quarterlyTaxRate` (from `EconomyConfig.quarterlyTaxRate`, default 5%)
> - Events fired: `DailySettlement` (EventType 550), `WeeklyReport` (551), `QuarterlySettlement` (552), `StarRatingReEvalRequested` (553)

**Settlement Schedule:**
- **Daily**: `DailySettlement` event — payload: `{day, income, expense, balance}`
- **Weekly (day % 7 == 0)**: `WeeklyReport` event alongside DailySettlement
- **Quarterly (day % 90 == 0)**: `QuarterlySettlement` event — `{quarter, taxAmount, balance}`, then `StarRatingReEvalRequested`

**Quarterly Settlement:**
1. Tax calculation: `tax = static_cast<int64_t>(balance_ * config_.quarterlyTaxRate)`
2. Tax deducted via `EconomyEngine::addExpense(tax, "quarterly_tax")`
3. `StarRatingReEvalRequested` event fired → `StarRatingSystem` re-evaluates on next update
4. `QuarterlySettlement` event with quarter index (day / 90)

**EconomyConfig fields (from balance.json):**
```cpp
struct EconomyConfig {
    int64_t startingBalance;
    int64_t elevatorMaintenancePerDay;
    float   quarterlyTaxRate;           // 0.05 = 5%
};
```

**UI Integration:**
- `HUDPanel` receives `DailySettlement` via EventBus → updates toast queue
- ImGui toast: settlement summary shown for 3 seconds

### 5.21 Game Over + Victory Conditions

> **Sprint 4 Implementation (2026-03-26~27, TASK-04-004):**
> - `GameOverSystem` — Layer 1 domain class, listens to `DayChanged` event
> - **Bankruptcy**: balance < 0 for 30 consecutive days → `GameOver` event (`reason: "bankruptcy"`)
> - **Mass Exodus**: activeAgentCount < 10% of `maxNpc` capacity for 7 consecutive days → `GameOver` event (`reason: "mass_exodus"`)
> - **TOWER Victory**: ★5 + 100 built floors + 300 active NPCs + 90 consecutive positive-balance days → `TowerAchieved` event
> - Both conditions mutually exclusive: once fired, `gameOverFired_` / `victoryFired_` = true, no further checks

**Game Over Conditions:**
| Condition | Trigger | Threshold |
|---|---|---|
| Bankruptcy | `consecutiveNegativeDays_ >= 30` | 30 days |
| Mass Exodus | `massExodusDays_ >= 7` | 7 days, NPC < 10% capacity |

**Victory Condition (TOWER):**
| Requirement | Value |
|---|---|
| Star rating | ★5 (`StarRating::Star5`) |
| Built floors | `grid_.builtFloorCount() >= 100` |
| Active NPCs | `agents_.activeAgentCount() >= 300` |
| Positive balance streak | `consecutivePositiveDays_ >= 90` |

**Event Payloads:**
```cpp
struct GameOverPayload {
    std::string reason;   // "bankruptcy" or "mass_exodus"
    int day;
    int64_t finalBalance;
};
struct TowerAchievedPayload {
    int day;
    int starRating;
    int floorCount;
    int npcCount;
};
```

**Bootstrapper Integration:**
- `GameOverSystem::update()` called on `DayChanged` event subscription
- `GameStateManager::transition(GameState::GameOver)` on GameOver event
- `GameStateManager::transition(GameState::Victory)` on TowerAchieved event

### 5.22 Main Menu + Game State Machine

> **Sprint 4 Implementation (2026-03-27, TASK-04-005):**
> - `GameStateManager` — Core Runtime class, owns `currentState_` (init: `MainMenu`)
> - States: `MainMenu / Playing / Paused / GameOver / Victory`
> - Transition guard: `canTransition(from, to)` — only valid edges allowed
> - `GameStateChanged` event (EventType 612) fired on every valid transition
> - ESC key → `TogglePause` command → `Playing↔Paused`
> - New Game: `MainMenu → Playing` (2-step: reset state → `setupInitialScene()`)

**State Machine (authoritative):**
```
MainMenu → Playing    (NewGame / LoadGame)
Playing  → Paused     (ESC / TogglePause)
Playing  → GameOver   (GameOver event)
Playing  → Victory    (TowerAchieved event)
Paused   → Playing    (Resume / TogglePause)
Paused   → MainMenu   (QuitToMenu)
GameOver → MainMenu   (NewGame)
Victory  → MainMenu   (NewGame)
```

**GameStateManager API:**
```cpp
class GameStateManager {
public:
    GameStateManager(EventBus& eventBus);
    GameState getState() const;
    bool transition(GameState newState);   // Returns false if invalid transition
    bool canTransition(GameState from, GameState to) const;
private:
    EventBus& eventBus_;
    GameState currentState_ = GameState::MainMenu;
};
```

**UI Implementation:**
- Main menu: Fullscreen ImGui overlay (game world not rendered)
- Pause menu: Semi-transparent ImGui modal, game view visible behind
- GameOver/Victory: Fullscreen ImGui modal with stats summary
- Bootstrapper checks `gameState_.getState()` each frame to determine render mode

### 5.23 DI-005 Resolution

> **Sprint 4 Implementation (2026-03-27, TASK-04-006):**
> - `MockGridSystem` used in tests treated elevator shaft tiles as anchor tiles (`isAnchor = true`)
> - Production `GridSystem` does NOT set `isAnchor` on elevator shaft tiles — shaft tiles have `isElevatorShaft = true`, `isAnchor = false`
> - Mismatch caused `EconomyEngine::payMaintenance()` to double-count elevator maintenance in tests

**Resolution:**
1. **MockGridSystem fix**: `isElevatorShaft` tiles now return `isAnchor = false` (matches production)
2. **Test isolation**: Each test file's `MockGridSystem` wrapped in `anonymous namespace` — resolves ODR violation causing vtable corruption in Release builds
3. **getTenantCount() test**: Added explicit test for `IGridSystem::getTenantCount()` (previously only tested implicitly)
4. **ODR fix commit**: `b051472` — anonymous namespace wrapping eliminated Release-mode test failures

**ODR Rule (mandatory for all test files):**
```cpp
// tests/test_*.cpp — ALL mock classes must be in anonymous namespace
namespace {
class MockGridSystem : public vse::IGridSystem { ... };
class MockAgentSystem : public vse::IAgentSystem { ... };
} // anonymous namespace
```

**Impact:**
- Eliminates elevator rendering glitches
- Ensures consistent NPC boarding behavior
- Improves test reliability for elevator-related features

### 5.24 BuildCursor (Sprint 5 Planned)

> **Sprint 5 Planned System (TASK-05-001):**
> - `BuildCursor` — Layer 3 renderer-side construct, tracks mouse hover tile and selected build mode
> - Hover highlight: tile under mouse cursor rendered with semi-transparent overlay
> - Left-click executes: `BuildFloor` or `PlaceTenant` command depending on selected mode
> - Tenant selection UI: B key → floor mode; T key → tenant selection popup (Office/Residential/Commercial)
> - Cost tooltip: shows build cost at cursor position
> - Invalid placement: red highlight when tile is occupied or floor not built

**BuildCursor State:**
```cpp
enum class BuildMode { None, Floor, Office, Residential, Commercial };

struct BuildCursorState {
    TileCoord hoverTile;
    BuildMode mode = BuildMode::None;
    bool isValidPlacement = false;
    int64_t previewCost = 0;
};
```

**Rendering:**
- Valid placement: semi-transparent green overlay on hovered tile
- Invalid: semi-transparent red overlay
- Tooltip: `"₩{cost}"` shown near cursor via ImGui

**Input → Command flow:**
- `InputMapper` → `GameCommand::SelectTile` carries `TileCoord`
- `BuildCursor` converts to `BuildFloor` / `PlaceTenant` command based on `BuildMode`
- Layer boundary: `BuildCursor` lives in `renderer/` — never calls domain directly; emits `GameCommand`

**Integration:**
- `RenderFrame` carries `BuildCursorState` from domain/Bootstrapper
- `SDLRenderer` draws cursor overlay before ImGui layer

### 5.25 Camera Zoom & Pan (Sprint 5 Planned)

> **Sprint 5 Planned System (TASK-05-002):**
> - Mouse wheel → zoom (in/out), min/max zoom clamped (configurable: `camera.zoomMin`, `camera.zoomMax`)
> - Right-click drag → pan (pan speed configurable: `camera.panSpeed`)
> - WASD keys → pan
> - Pan boundary: camera cannot scroll past building extents + margin
> - Zoom pivot: zoom centered on mouse cursor position (not screen center)

**Camera State (extends existing `Camera` class):**
```cpp
class Camera {
public:
    void zoomAt(float delta, PixelPos mousePos);    // Zoom centered on mouse
    void pan(float dx, float dy);                   // Pan in screen units
    void clampToWorld(int worldW, int worldH);      // Enforce boundary
    float zoom() const;
    PixelPos offset() const;
    TileCoord screenToTile(PixelPos screen, int tileSize) const;
    PixelPos tileToScreen(TileCoord tile, int tileSize) const;
};
```

**Input Mapping:**
| Input | Action |
|---|---|
| Mouse wheel up/down | `ZoomIn` / `ZoomOut` command |
| Right-click drag | `PanCamera{dx, dy}` command |
| W/A/S/D | `PanCamera` per-frame |
| Middle-click drag | Alternative pan |

**Config (game_config.json):**
```json
"camera": {
    "zoomMin": 0.25,
    "zoomMax": 4.0,
    "panSpeed": 8.0,
    "zoomStep": 0.1
}
```

### 5.26 Save/Load UI (Sprint 5 Planned)

> **Sprint 5 Planned System (TASK-05-003):**
> - Save slot list: up to N slots, each showing metadata (day, balance, star rating, timestamp)
> - Manual save: pause menu → "Save" → slot selection popup
> - Manual load: main menu "Load Game" → slot selection → `SaveLoadSystem::load(slotIndex)`
> - Auto-save: every 60 game days, slot 0 reserved
> - Overwrite confirmation modal when saving to existing slot

**Save Slot Metadata (displayed in UI):**
```cpp
struct SaveSlotInfo {
    int slotIndex;
    bool isEmpty;
    SaveMetadata meta;    // day, balance, starRating, timestamp (ISO 8601)
    std::string displayName;  // e.g., "Day 45 ★3 ₩1,234,567"
};
```

**UI Flow:**
```
Pause Menu → [Save]
  → SlotList popup (ImGui)
    → Select slot → OverwriteConfirm? → SaveLoadSystem::save(slot)
  → Toast: "Game Saved (Slot {n})"

Main Menu → [Load Game]
  → SlotList popup
    → Select slot → SaveLoadSystem::load(slot)
    → GameStateManager::transition(Playing)
```

**Auto-save:**
- Bootstrapper subscribes to `DayChanged` → every 60 days calls `SaveLoadSystem::save(0)` (auto-save slot)
- Silent (no modal), toast notification only

### 5.27 HUD 고도화 (Sprint 5 Planned)

> **Sprint 5 Planned System (TASK-05-004):**
> - Construction toolbar: bottom HUD with Floor / Office / Residential / Commercial buttons
> - Star rating: animated ★☆ icons in top bar
> - Daily profit/loss indicator: green ↑ / red ↓ next to balance
> - Toast queue: up to 3 simultaneous toasts (settlement, build, error), auto-dismiss 3s
> - Game speed control: ×1 / ×2 / ×3 buttons in HUD

**HUD Layout (ImGui):**
```
┌─────────────────────────────────────────────────────┐
│ [Day 45]  ★★★☆☆   ₩1,234,567 ↑₩12,000   [⏸][×1][×2][×3] │ ← Top bar
│                                                           │
│              (game world)                                 │
│                                                           │
│  [🏢Floor] [🏢Office] [🏠Residential] [🏪Commercial]      │ ← Bottom toolbar
└─────────────────────────────────────────────────────┘
```

**Toast System:**
```cpp
struct ToastMessage {
    std::string text;
    float remainingSeconds;
    ToastType type;   // Info / Warning / Error
};
// Max 3 toasts stacked; oldest dismissed first on overflow
```

**Speed Control:**
- `×1 / ×2 / ×3` buttons → `GameCommand::SetSpeed{n}` → `SimClock::setSpeed(n)`
- Current speed button highlighted

**Toolbar → BuildCursor integration:**
- Clicking toolbar button sets `BuildMode` in `BuildCursor`
- Active mode button highlighted

### 5.28 TD-001 isAnchor Refactoring (Sprint 5 Planned)

> **Sprint 5 Planned System (TASK-05-005):**
> - Tech debt: `TileData.isAnchor` is overloaded — used for both tenant anchor AND elevator shaft identification
> - Separation: `isElevatorShaft` already exists on `TileData`; ensure no code uses `isAnchor` to identify elevator shafts
> - `GridSystem`, `EconomyEngine` audit: replace `isAnchor` elevator-shaft checks with `isElevatorShaft`
> - `MockGridSystem` in tests: verify anchor/shaft fields consistent post-DI-005 fix

**Refactoring Rules:**
| Field | Semantics |
|---|---|
| `TileData.isAnchor` | True ONLY for leftmost tile of a multi-tile tenant |
| `TileData.isElevatorShaft` | True ONLY for elevator shaft tiles |
| These are MUTUALLY EXCLUSIVE | Elevator shaft tiles must NEVER have `isAnchor = true` |

**Audit Checklist:**
```bash
# Any isAnchor usage near elevator-related logic = bug
grep -rn "isAnchor" src/ include/ | grep -v "test_"
# Cross-check with isElevatorShaft usage
grep -rn "isElevatorShaft" src/ include/
```

**Expected changes:**
- `EconomyEngine::payMaintenance()`: iterate elevator shafts by `isElevatorShaft`, not `isAnchor`
- `BuildCursor` (Sprint 5): highlight logic uses `isElevatorShaft` to block tenant placement
- No behavior change — correctness already ensured by DI-005 fix; this is naming/intent clarity

### 5.29 Tile Sprite Rendering (Sprint 6 Planned)

> **Sprint 6 Planned System (TASK-06-001):**
> Replace colored rectangle tile rendering with actual sprite textures (SimTower style pixel art).
> `SpriteSheet` already exists for NPCs — extend pattern to building tiles.

**Tile Texture Map:**

| TenantType | Sprite File | Size |
|---|---|---|
| `Office` | `content/sprites/office_floor.png` | 32×32 |
| `Residential` | `content/sprites/residential_floor.png` | 32×32 |
| `Commercial` | `content/sprites/commercial_floor.png` | 32×32 |
| `Lobby` (floor 0) | `content/sprites/lobby_floor.png` | 32×32 |
| `ElevatorShaft` | `content/sprites/elevator_shaft.png` | 32×32 |
| `Empty` | *(no sprite, background color)* | — |
| Building facade (wall) | `content/sprites/building_facade.png` | 32×32 |

**TileRenderer class (Layer 3):**
```cpp
// include/renderer/TileRenderer.h
class TileRenderer {
public:
    TileRenderer(SDL_Renderer* renderer);

    /// Load all tile textures. Missing files fall back to colored rects.
    bool loadTextures(const std::string& contentDir);

    /// Draw a single tile at screen position.
    void drawTile(const RenderTileCmd& cmd, float screenX, float screenY,
                  float scaledSize, const Camera& cam);

private:
    SDL_Renderer* renderer_;
    std::unordered_map<int, SDL_Texture*> textures_;  // key = TenantType (int)
    SDL_Texture* facadeTexture_  = nullptr;
    SDL_Texture* shaftTexture_   = nullptr;

    void drawFallback(TenantType type, float x, float y, float size);
};
```

**Fallback policy:** If a texture file is missing, draw the existing colored rect (no crash).
**Layer boundary:** TileRenderer lives in `src/renderer/` — no domain headers.
**SDLRenderer integration:** Replace `drawTiles()` colored rect logic with `tileRenderer_.drawTile()`.

### 5.30 Elevator Sprite Animation (Sprint 6 Planned)

> **Sprint 6 Planned System (TASK-06-002):**
> Animate elevator car movement and door open/close with sprites.

**Elevator sprite files:**
- `content/sprites/elevator_car.png` — 32×32, door closed
- `content/sprites/elevator_open.png` — 32×32, door open

**Animation states:**
| `ElevatorState` | Sprite | Notes |
|---|---|---|
| `Idle` | `elevator_car.png` | Static at floor |
| `MovingUp` / `MovingDown` | `elevator_car.png` | `interpolatedFloor` for smooth Y |
| `DoorOpening` | Lerp car→open | Transition animation (2 frames) |
| `Boarding` | `elevator_open.png` | Doors fully open |
| `DoorClosing` | Lerp open→car | Transition animation (2 frames) |

**Implementation:** `ElevatorRenderer` class (Layer 3), called from `SDLRenderer::drawElevators()`.

### 5.31 NPC Sprite Sheet — Real Asset Integration (Sprint 6 Planned)

> **Sprint 6 Planned System (TASK-06-003):**
> `SpriteSheet` class already implemented (Sprint 3). Sprint 6 provides the actual PNG asset
> (`content/sprites/npc_sheet.png`, 128×32, 8 frames × 16px) to replace placeholder.
> No code changes required if file is placed in correct path.
> If asset is unavailable, `SpriteSheet` falls back to colored rects automatically.

**Frame mapping (already implemented):**
| Frame | AgentState |
|---|---|
| 0–3 | `Walking` (walk cycle) |
| 4 | `Idle` / `Working` |
| 5 | `InElevator` |
| 6 | `Resting` |
| 7 | `UsingStairs` |

### 5.32 Audio Engine — IAudioCommand Implementation (Sprint 6 Planned)

> **Sprint 6 Planned System (TASK-06-004):**
> Implement `AudioEngine` (Layer 3) that consumes `IAudioCommand` structs.
> Backend: SDL2_mixer. IAudioCommand already defined in `include/core/IAudioCommand.h`.

**AudioEngine class:**
```cpp
// include/renderer/AudioEngine.h
#pragma once
#include "core/IAudioCommand.h"
#include <string>
#include <unordered_map>

namespace vse {

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    /// Initialize SDL2_mixer. Returns false if unavailable (graceful degradation).
    bool init(int frequency = 44100, int channels = 2, int chunkSize = 2048);
    void shutdown();

    /// Load audio assets from content directory.
    void loadAssets(const std::string& contentDir);

    /// Execute audio commands (call every frame from Bootstrapper).
    void process(const PlayBGMCmd& cmd);
    void process(const PlaySFXCmd& cmd);
    void process(const StopBGMCmd& cmd);

    bool isAvailable() const { return available_; }

private:
    bool available_ = false;
    // Mix_Music* bgmTrack_ = nullptr;  // SDL2_mixer type (forward-declared)
    std::unordered_map<std::string, void*> sfxCache_;  // sfxId → Mix_Chunk*

    void playBGM(const std::string& trackId, float volume, bool loop);
    void playSFX(const std::string& sfxId, float volume);
};

} // namespace vse
```

**Graceful degradation:** If SDL2_mixer is unavailable or audio file missing, `AudioEngine::init()` returns false and all `process()` calls are no-ops. Game runs silently without crash.

**Audio assets (content/audio/):**
| File | Type | Trigger |
|---|---|---|
| `bgm_daytime.ogg` | BGM | Game hour 6–20 |
| `bgm_night.ogg` | BGM | Game hour 20–6 |
| `sfx_elevator_ding.wav` | SFX | `ElevatorArrived` event |
| `sfx_build.wav` | SFX | `BuildFloor` command |
| `sfx_tenant_in.wav` | SFX | `TenantPlaced` event |

**Bootstrapper integration:**
- `AudioEngine audioEngine_` added as member
- `init()` calls `audioEngine_.init()` + `audioEngine_.loadAssets()`
- Main loop: check `pendingBGM_` / `pendingSFX_` from RenderFrame or event subscriptions

**EventBus subscriptions (in Bootstrapper::init()):**
```cpp
eventBus_.subscribe(EventType::ElevatorArrived, [this](const Event&) {
    pendingSFX_ = "sfx_elevator_ding";
});
// Day/Night BGM switch via HourChanged event
```

### 5.33 Day/Night Visual Cycle (Sprint 6 Planned)

> **Sprint 6 Planned System (TASK-06-005):**
> Render the game world with a day/night color overlay based on `GameTime.hour`.

**Color overlay rules:**
| Hour | Overlay | Notes |
|---|---|---|
| 6–8 | Warm orange tint (dawn) | Alpha 40 |
| 8–18 | No overlay (daytime) | Clear |
| 18–20 | Warm orange tint (dusk) | Alpha 40 |
| 20–6 | Dark blue overlay (night) | Alpha 60 |

**Implementation:** `SDLRenderer` applies `SDL_SetRenderDrawColor + SDL_RenderFillRect` overlay
after drawing tiles/agents, before ImGui.
`RenderFrame` carries `frame.gameHour` (int) for renderer to compute overlay.

### 5.34 Building Facade Rendering (Sprint 6 Planned)

> **Sprint 6 Planned System (TASK-06-006):**
> Draw building exterior walls (left/right sides) using `building_facade.png` tile.
> Currently: plain gray rectangles.

**Facade rules:**
- Left wall: X=0, every built floor — `building_facade.png` drawn at left edge
- Right wall: X=floorWidth-1, every built floor
- Unbuilt floors: no facade (dark/empty)

### 5.35 SDL2_mixer CMake Integration (Sprint 6 Planned)

> **Sprint 6 Planned System (TASK-06-007):**
> Add `SDL2_mixer` as optional dependency in `cmake/FetchDependencies.cmake`.
> If not found, define `VSE_NO_AUDIO` and compile with stub AudioEngine.

```cmake
# cmake/FetchDependencies.cmake addition
find_package(SDL2_mixer QUIET)
if(SDL2_mixer_FOUND)
    target_link_libraries(TowerTycoon PRIVATE SDL2_mixer::SDL2_mixer)
    target_compile_definitions(TowerTycoon PRIVATE VSE_HAS_AUDIO)
else()
    message(STATUS "SDL2_mixer not found — audio disabled")
endif()
```

### 5.36 Sprint 6 Task List

| Task | Title | Assignee | Difficulty |
|---|---|---|---|
| TASK-06-000 | Design Spec v3.0 동기화 | 붐 | ⭐⭐ |
| TASK-06-001 | 타일 스프라이트 렌더링 | 붐2 | ⭐⭐⭐ |
| TASK-06-002 | 엘리베이터 애니메이션 | 붐2 | ⭐⭐ |
| TASK-06-003 | NPC 실제 스프라이트 연동 | 붐2 | ⭐ |
| TASK-06-004 | AudioEngine (SDL2_mixer) | 붐2 | ⭐⭐⭐ |
| TASK-06-005 | 낮/밤 비주얼 사이클 | 붐2 | ⭐⭐ |
| TASK-06-006 | 건물 외벽 렌더링 | 붐2 | ⭐ |
| TASK-06-007 | SDL2_mixer CMake 통합 | 붐2 | ⭐ |
| TASK-06-008 | 통합 + Sprint 6 빌드 | 붐 | ⭐⭐⭐ |

**전제 조건:** 마스터가 픽셀아트 에셋 (`content/sprites/*.png`, `content/audio/*.ogg/*.wav`) 준비 완료 후 TASK-06-001~003 시작 가능. TASK-06-004~007은 에셋 없이 코드 먼저 구현 가능.

---

## 6. ECS Component & System Map

### 6.1 Components (all data-only)

| Component | Fields | Used By |
|---|---|---|
| `PositionComponent` | tile, pixel, facing | AgentSystem, Renderer |
| `AgentComponent` | state, homeTenant, workplaceTenant, satisfaction, moveSpeed, stress | AgentSystem, Economy |
| `AgentScheduleComponent` | workStartHour, workEndHour, lunchHour | AgentSystem |
| `AgentPathComponent` | path[], currentIndex, destination | AgentSystem |
| `ElevatorPassengerComponent` | targetFloor, waiting | TransportSystem |
| `TenantComponent` | type, anchorTile, width, rentPerDay, maintenanceCost, maxOccupants, occupantCount, buildCost, satisfactionDecayRate, isEvicted, evictionCountdown | EconomyEngine, GridSystem, TenantSystem |
| `ElevatorComponent` | shaftX, bottomFloor, topFloor, capacity, currentFloor, interpolatedFloor, state, direction, callQueue, passengers | TransportSystem |
| `StarRatingComponent` (singleton) | currentRating, avgSatisfaction, totalPopulation | StarRatingSystem |
| `BuildingComponent` (singleton) | name, builtFloors, totalTenants | GridSystem |

### 6.2 Additional Component Definitions

```cpp
struct TenantComponent {
    TenantType  type            = TenantType::COUNT;
    TileCoord   anchorTile      = {0, 0};
    int         width           = 1;
    int64_t     rentPerDay      = 0;      // Cents
    int64_t     maintenanceCost = 0;      // Cents per day
    int         maxOccupants    = 0;
    int         occupantCount   = 0;      // Current occupant NPC count
    int64_t     buildCost       = 0;      // Cents (one-time)
    float       satisfactionDecayRate = 0.0f; // Per-tick rate for NPC satisfaction
    bool        isEvicted       = false;
    int         evictionCountdown = 0;    // Ticks remaining before despawn (0 = not evicting)
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
4. agentSystem_->update(reg, time)   — NPC AI decisions, pathfinding, movement, stress/satisfaction
5. transportSystem_->update(reg, time) — Elevator FSM + LOOK algorithm
6. tenantSystem_->update(reg, time)  — Eviction countdown, satisfaction checks
7. economyEngine_->update(reg, time) — Rent/maintenance on DayChanged, periodic settlements
8. starRatingSystem_->update(reg, time) — Recalculate satisfaction average
9. contentRegistry_->checkAndReload()   — Hot-reload (every N ticks, not every tick)
--- (outside tick loop) ---
10. render()                          — Collect RenderFrame → SDLRenderer (1x per frame)
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
        TenantSystem
             │
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
- **ConfigManager**: depended on by GridSystem, AgentSystem, Transport, TenantSystem, Economy. No deps.
- **SimClock** → EventBus only
- **GridSystem** → EventBus, ConfigManager
- **AgentSystem** → EventBus, ConfigManager, GridSystem (read-only), TransportSystem (call elevator)
- **TransportSystem** → EventBus, ConfigManager
- **TenantSystem** → EventBus, ConfigManager, GridSystem, IEconomyEngine
- **EconomyEngine** → EventBus, ConfigManager, GridSystem (read tenant info), TenantSystem
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
 9. TenantSystem      — Needs EventBus, ConfigManager, GridSystem, IEconomyEngine
10. EconomyEngine     — Needs EventBus, ConfigManager, GridSystem, TenantSystem
11. StarRatingSystem  — Needs EventBus, AgentSystem
12. SaveLoad          — Needs all above (serializes everything)
13. SDLRenderer       — Needs ConfigManager (window size). SDL_Init here.
14. InputMapper       — Needs SDLRenderer context
15. DebugPanel        — Needs SDLRenderer (ImGui context)

Shutdown: reverse order (15 → 1)

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

> 경로: `assets/data/balance.json` (TASK-02-003에서 `assets/config/` → `assets/data/`로 이동 확정)
>
> EconomyConfig 로딩 키 매핑:
> - `startingBalance` ← `economy.startingBalance`
> - `officeRentPerTilePerDay` ← `tenants.office.rent`
> - `residentialRentPerTilePerDay` ← `tenants.residential.rent`
> - `commercialRentPerTilePerDay` ← `tenants.commercial.rent`
> - `elevatorMaintenancePerDay` ← `economy.elevatorMaintenancePerDay`

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
        "startingBalance": 1000000,
        "elevatorMaintenancePerDay": 1000
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
| 2026-03-24 | 1.4 | External review (DeepSeek R1 + GPT-5.4): (3.2) SaveLoad entity cross-reference safety — removed "automatically safe" assertion, added round-trip test requirements, clarified restore sequence (ECS first → non-ECS → derived caches); (3.3) spawnAgent() redesigned to `(reg, homeTenantId, workplaceId, optional spawnPos)` — EntityId-based; (3.4) TransportSystem internal separation guide added (ElevatorCarFSM + LookScheduler); (3.5) PixelPos x conversion formula added; (3.6) CLAUDE.md version synced to v1.3. |
| 2026-03-26 | 1.5 | Sprint 2 구현 반영 동기화: **(1) Bootstrapper §5.15** — API 시그니처 실제 구현으로 전면 교체 (`initialize` → `init/run/shutdown/initDomainOnly`, `unique_ptr<I*>` 소유 → 직접 멤버, `#ifdef VSE_TESTING` 접근자 가드, `setupInitialScene()` 공용 헬퍼, `accumulator_` Bootstrapper 소유 명시); **(2) ISaveLoad §5.8** — `SaveMetadata.balance` `int` → `int64_t` (TASK-02-003 Cents 단위 통일); **(3) Types.h §2** — `StarRatingChangedPayload` struct 추가 (TASK-02-009: StarRatingSystem.h에서 이동, 레이어 결합 감소); **(4) SaveLoad 직렬화 전략** — EnTT snapshot → 수동 JSON + MessagePack 2-pass remap으로 확정 반영 (이미 §5.8에 있었으나 이유 보강); **(5) SimClock §5.1** — 주석/설계 메모 실제 구현과 일치 확인 (변경 없음). |
| 2026-03-26 | 2.0 | Sprint 3 implementation sync + Sprint 4 planned systems: **(1) §5.16 NPC Stress System** — AgentComponent.stress field (0-100), satisfaction decay when stress > 50, AgentSatisfactionChanged event, leave threshold satisfaction < 10; **(2) §5.17 TenantSystem** — TenantComponent fields (anchorTile, rentPerDay, maintenanceCostPerDay, evictionCountdown), onDayChanged() rent/maintenance, eviction reset logic; **(3) §5.18 Economy Loop** — buildFloor cost deduction, collectRent/payMaintenance auto-call, InsufficientFundsEvent payload; **(4) §5.19 Stair Movement System** — ≤4 floor stair preference, 2 ticks/floor speed, elevator wait fallback; **(5) §5.20 Periodic Settlement** — daily/weekly/quarterly settlement schedule, tax deduction, ImGui toast; **(6) §5.21 Game Over + Victory** — bankruptcy 30-day rule, TOWER achievement (★5 + 100 floors + 300 NPCs); **(7) §5.22 Main Menu + State Machine** — Menu/Playing/Paused/GameOver states, ImGui fullscreen menu; **(8) §5.23 DI-005 Resolution** — MockGridSystem elevator anchor mismatch fix; **(9) Types.h updates** — InsufficientFunds event type, InsufficientFundsPayload struct, AgentComponent.stress field, TenantComponent full definition. |
| 2026-03-27 | 2.5 | Sprint 4 구현 반영 동기화 + Sprint 5 계획 섹션 추가: **(1) §5.18 Economy Loop** — 실제 구현 반영 (TenantSystem.onDayChanged→collectRent/payMaintenance, EconomyConfig.quarterlyTaxRate 필드 추가); **(2) §5.19 Stair Movement System** — UsingStairs 전용 AgentState, 2 ticks/floor, elevatorWaitTimeout 폴백, orange 렌더링 확정; **(3) §5.20 Periodic Settlement** — EconomyConfig.quarterlyTaxRate, EventType 550~553 확정, 페이로드 구조체 동기화; **(4) §5.21 Game Over + Victory** — GameOverSystem (Layer 1) 구현 반영, consecutiveNegativeDays/massExodusDays/consecutivePositiveDays 추적, 상호배제 플래그; **(5) §5.22 Main Menu + State Machine** — GameStateManager Core Runtime 확정, canTransition() guard, 2-step NewGame 전환; **(6) §5.23 DI-005** — anonymous namespace ODR 필수화 (b051472), isElevatorShaft→isAnchor=false 확정; **(7) §5.15 Bootstrapper** — GameOverSystem + GameStateManager 멤버 추가; **(8) §5.24~5.28 Sprint 5 계획** — BuildCursor, Camera Zoom/Pan, Save/Load UI, HUD 고도화 (toolbar/toast/속도제어), TD-001 isAnchor 리팩토링 신설. |
| 2026-03-27 | 3.0 | Sprint 5 구현 반영 동기화 + Sprint 6 계획 섹션 추가: **(1) §5.24 BuildCursor** — drawOverlay/drawImGui 분리(ImGui lifecycle 수정), startX clamp 일관성 확정; **(2) §5.25 Camera Zoom/Pan** — zoomAt pivot 알고리즘, clampToWorld UB 수정(world<viewport 시 centering), panMargin_ 캐시; **(3) §5.26 Save/Load UI** — SaveSlotInfo Layer 0(IRenderCommand.h) 확정, getSavePath() API 일관성, openSave() 매 프레임 재호출 방지; **(4) §5.27 HUD 고도화** — 툴바/토스트/배속 버튼 Bootstrapper 연결 확정, DailySettlement 구독 위치 수정; **(5) §5.28 TD-001** — isAnchor/isElevatorShaft 상호배제 전 코드베이스 적용 완료; **(6) §5.29~5.35 Sprint 6 계획 신설** — TileRenderer(타일 스프라이트), 엘리베이터 애니메이션, NPC 실제 스프라이트 연동, AudioEngine(SDL2_mixer), 낮/밤 비주얼 사이클, 건물 외벽 렌더링, SDL2_mixer CMake 통합; **(7) §5.36 Sprint 6 태스크 목록** — 9개 태스크 정의(에셋 없이 선행 가능 태스크 포함). |

---
*This document is maintained by 붐 (PM). Changes require Human approval.*
*Implementation must follow CLAUDE.md constraints. This doc provides the HOW.*
