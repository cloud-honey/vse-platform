# VSE Design Specification — Phase 1
> Version: 1.0 | Author: 붐 (PM) | Date: 2026-03-24
> Source of truth: `CLAUDE.md` v1.1 | This document details HOW to implement what CLAUDE.md defines.
> CLAUDE.md = WHAT (rules, constraints) | This doc = HOW (signatures, data flow, init order)

---

## 1. Directory Structure & File Map

```
vse-platform/
├── CMakeLists.txt                    # Root build
├── cmake/
│   └── FetchDependencies.cmake       # All FetchContent declarations
│
├── include/
│   ├── core/                         # Layer 0 — Interfaces only
│   │   ├── IGridSystem.h
│   │   ├── IAgentSystem.h
│   │   ├── ITransportSystem.h
│   │   ├── IEconomyEngine.h
│   │   ├── ISaveLoad.h
│   │   ├── IAsyncLoader.h
│   │   ├── IMemoryPool.h
│   │   ├── INetworkAdapter.h         # Empty — Phase 3
│   │   ├── IAuthority.h              # Empty — Phase 3
│   │   ├── ISpatialPartitioning.h    # Declaration only
│   │   ├── IRenderCommand.h
│   │   ├── IAudioCommand.h
│   │   ├── SimClock.h
│   │   ├── EventBus.h
│   │   ├── ConfigManager.h
│   │   ├── ContentRegistry.h
│   │   ├── LocaleManager.h
│   │   ├── Bootstrapper.h
│   │   ├── Types.h                   # Shared types, enums, constants
│   │   └── Error.h                   # Error codes, Result<T> alias
│   │
│   ├── domain/                       # Layer 1 — RealEstateDomain
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
│   └── renderer/                     # Layer 3 — SDL2
│       ├── SDLRenderer.h
│       ├── TileRenderer.h
│       ├── AgentRenderer.h
│       ├── UIRenderer.h
│       └── DebugPanel.h              # Dear ImGui
│
├── src/
│   ├── core/                         # Layer 0 implementations (non-interface)
│   │   ├── SimClock.cpp
│   │   ├── EventBus.cpp
│   │   ├── ConfigManager.cpp
│   │   ├── ContentRegistry.cpp
│   │   ├── LocaleManager.cpp
│   │   └── Bootstrapper.cpp
│   │
│   ├── domain/                       # Layer 1
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
│   └── test_ConfigManager.cpp
│
├── .github/
│   └── workflows/
│       └── build.yml
│
├── tasks/                            # Task specs (붐 → DevTeam)
├── docs/                             # Design docs
│   └── VSE_Design_Spec.md           # This file (symlink or copy)
│
├── CLAUDE.md                         # Implementation spec (source of truth)
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

namespace vse {

// ── Coordinate ──────────────────────────────
// Origin: bottom-left. Y increases upward. floor 0 = ground.
struct TileCoord {
    int x;
    int floor;      // 0 = ground, negative = basement (Phase 2)

    bool operator==(const TileCoord& o) const { return x == o.x && floor == o.floor; }
    bool operator!=(const TileCoord& o) const { return !(*this == o); }
};

// Pixel position (render space). Origin: top-left (SDL2 convention).
// Conversion: pixelY = (MAX_FLOORS - 1 - floor) * TILE_SIZE
struct PixelPos {
    float x;
    float y;
};

// ── Entity IDs ──────────────────────────────
using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY = 0;

// ── Time ────────────────────────────────────
using SimTick = uint64_t;        // Monotonic tick counter
using GameMinute = uint32_t;     // In-game minutes since day 0

struct GameTime {
    int day;         // 0-indexed
    int hour;        // 0-23
    int minute;      // 0-59

    GameMinute toMinutes() const { return day * 1440 + hour * 60 + minute; }
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

    // System
    GameSaved = 900,
    GameLoaded,
    ConfigReloaded,
};

// ── Constants (defaults — override via game_config.json) ────
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
    ConfigLoadFailed,
    SaveFailed,
    LoadFailed,
    ContentNotFound,
    InvalidTenantType,
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

## 4. Layer 0 Interface Specifications

### 4.1 SimClock

```cpp
// include/core/SimClock.h
#pragma once
#include "core/Types.h"
#include <functional>
#include <vector>

namespace vse {

class EventBus;  // forward declaration

// SimClock: 100ms fixed tick. Decoupled from rendering.
// Owns game time progression. Speed multiplier controls ticks-per-update.
class SimClock {
public:
    explicit SimClock(EventBus& bus);

    void update(double realDeltaMs);    // Called every frame with real elapsed ms
    void pause();
    void resume();
    void setSpeed(int multiplier);      // 1x, 2x, 4x (ticks per update)

    // Getters
    SimTick currentTick() const;
    GameTime currentGameTime() const;
    int speed() const;
    bool isPaused() const;

private:
    EventBus& eventBus_;
    SimTick tick_ = 0;
    double accumulator_ = 0.0;         // ms accumulated since last tick
    int speed_ = 1;                     // ticks per update cycle
    bool paused_ = false;

    void advanceTick();                 // Emit TickAdvanced + check day/hour change
};

} // namespace vse
```

### 4.2 EventBus

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
    std::any payload;           // Type-erased data
};

using EventCallback = std::function<void(const Event&)>;
using SubscriptionId = uint32_t;

// Single EventBus. Phase 3 will split into Local/State channels.
class EventBus {
public:
    SubscriptionId subscribe(EventType type, EventCallback callback);
    void unsubscribe(SubscriptionId id);
    void publish(Event event);

    // Process deferred events (call once per tick, after all systems update)
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
    SubscriptionId nextId_ = 1;
};

} // namespace vse
```

### 4.3 ConfigManager

```cpp
// include/core/ConfigManager.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace vse {

// Loads game_config.json at startup. Provides typed getters.
// Hot-reload for balance.json via ContentRegistry.
class ConfigManager {
public:
    Result<bool> loadFromFile(const std::string& path);

    // Typed getters with defaults
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

### 4.4 IGridSystem (Interface) + GridSystem (Implementation)

```cpp
// include/core/IGridSystem.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <optional>
#include <vector>

namespace vse {

// Tile data stored per grid cell
struct TileData {
    TenantType tenantType = TenantType::COUNT;  // COUNT = empty
    EntityId tenantEntity = INVALID_ENTITY;
    bool isLobby = false;
    bool isElevatorShaft = false;
    int tileWidth = 1;                          // Tenants can span multiple tiles
};

class IGridSystem {
public:
    virtual ~IGridSystem() = default;

    // Grid dimensions
    virtual int maxFloors() const = 0;
    virtual int floorWidth() const = 0;     // tiles per floor

    // Build operations
    virtual Result<bool> buildFloor(int floor) = 0;
    virtual bool isFloorBuilt(int floor) const = 0;
    virtual int builtFloorCount() const = 0;

    // Tile operations
    virtual Result<bool> placeTenant(TileCoord pos, TenantType type, int width, EntityId entity) = 0;
    virtual Result<bool> removeTenant(TileCoord pos) = 0;
    virtual std::optional<TileData> getTile(TileCoord pos) const = 0;
    virtual bool isTileEmpty(TileCoord pos) const = 0;
    virtual bool isValidCoord(TileCoord pos) const = 0;

    // Range query: all tiles on a floor
    virtual std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const = 0;

    // Elevator shaft
    virtual Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) = 0;
    virtual bool isElevatorShaft(TileCoord pos) const = 0;

    // Pathfinding support
    virtual std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const = 0;
};

} // namespace vse
```

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

    // IGridSystem implementation
    int maxFloors() const override;
    int floorWidth() const override;
    Result<bool> buildFloor(int floor) override;
    bool isFloorBuilt(int floor) const override;
    int builtFloorCount() const override;
    Result<bool> placeTenant(TileCoord pos, TenantType type, int width, EntityId entity) override;
    Result<bool> removeTenant(TileCoord pos) override;
    std::optional<TileData> getTile(TileCoord pos) const override;
    bool isTileEmpty(TileCoord pos) const override;
    bool isValidCoord(TileCoord pos) const override;
    std::vector<std::pair<TileCoord, TileData>> getFloorTiles(int floor) const override;
    Result<bool> placeElevatorShaft(int x, int bottomFloor, int topFloor) override;
    bool isElevatorShaft(TileCoord pos) const override;
    std::optional<TileCoord> findNearestEmpty(TileCoord from, int searchRadius) const override;

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

### 4.5 IAgentSystem (Interface) + ECS Components

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
    TileCoord tile;             // Current grid position
    PixelPos pixel;             // Interpolated render position
    Direction facing = Direction::Right;
};

struct AgentComponent {
    AgentState state = AgentState::Idle;
    TenantType workplace = TenantType::COUNT;   // Where this NPC works/lives
    EntityId homeTenant = INVALID_ENTITY;        // Tenant entity they belong to
    float satisfaction = 100.0f;                  // 0-100
    float moveSpeed = 1.0f;                       // tiles per second
};

struct AgentScheduleComponent {
    int workStartHour = 9;
    int workEndHour = 18;
    int lunchHour = 12;
    // Agent daily cycle: home → work → lunch → work → home
};

struct AgentPathComponent {
    std::vector<TileCoord> path;    // Ordered waypoints
    int currentIndex = 0;
    TileCoord destination;
};

struct ElevatorPassengerComponent {
    int targetFloor;                 // Where they want to go
    bool waiting = true;             // true = waiting at shaft, false = inside car
};

// ── Agent System Interface ──────────────────
class IAgentSystem {
public:
    virtual ~IAgentSystem() = default;

    // Lifecycle
    virtual Result<EntityId> spawnAgent(entt::registry& reg, TileCoord spawnPos, TenantType workplace, EntityId homeTenant) = 0;
    virtual void despawnAgent(entt::registry& reg, EntityId id) = 0;
    virtual int activeAgentCount() const = 0;

    // Per-tick update
    virtual void update(entt::registry& reg, const GameTime& time, float dt) = 0;

    // Query
    virtual std::optional<AgentState> getState(entt::registry& reg, EntityId id) const = 0;
    virtual std::vector<EntityId> getAgentsOnFloor(entt::registry& reg, int floor) const = 0;
    virtual std::vector<EntityId> getAgentsInState(entt::registry& reg, AgentState state) const = 0;
};

} // namespace vse
```

### 4.6 ITransportSystem (Elevator)

```cpp
// include/core/ITransportSystem.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <vector>
#include <optional>

namespace vse {

struct ElevatorState {
    EntityId id;
    int currentFloor;
    int targetFloor;                // -1 if idle
    ElevatorDirection direction;
    int passengerCount;
    int capacity;
    std::vector<EntityId> passengers;
    std::vector<int> callQueue;     // Floors to visit (LOOK ordered)
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

    // Per-tick update — LOOK algorithm
    virtual void update(float dt) = 0;

    // Query
    virtual std::optional<ElevatorState> getElevatorState(EntityId id) const = 0;
    virtual std::vector<EntityId> getElevatorsAtFloor(int floor) const = 0;
    virtual int getWaitingCount(int floor) const = 0;
    virtual std::vector<EntityId> getAllElevators() const = 0;
};

} // namespace vse
```

### 4.7 IEconomyEngine

```cpp
// include/core/IEconomyEngine.h
#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include <vector>

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

    // Balance
    virtual int getBalance() const = 0;
    virtual void addIncome(EntityId tenant, TenantType type, int amount) = 0;
    virtual void addExpense(const std::string& category, int amount) = 0;

    // Rent collection (called per game-day by SimClock)
    virtual void collectRent() = 0;
    virtual void payMaintenance() = 0;

    // Per-tick update
    virtual void update(const GameTime& time) = 0;

    // Stats
    virtual int getDailyIncome() const = 0;
    virtual int getDailyExpense() const = 0;
    virtual std::vector<IncomeRecord> getRecentIncome(int count) const = 0;
    virtual std::vector<ExpenseRecord> getRecentExpenses(int count) const = 0;
};

} // namespace vse
```

### 4.8 ISaveLoad

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

    // Quick save/load to default slot
    virtual Result<bool> quickSave() = 0;
    virtual Result<bool> quickLoad() = 0;

    // Metadata without full load
    virtual Result<SaveMetadata> readMetadata(const std::string& filepath) const = 0;

    // List available saves
    virtual std::vector<std::string> listSaves() const = 0;
};

} // namespace vse
```

### 4.9 ContentRegistry

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

// Manages JSON content packs. Supports hot-reload for balance tuning.
class ContentRegistry {
public:
    Result<bool> loadContentPack(const std::string& directory);

    // Typed content access
    const nlohmann::json& getTenantDef(TenantType type) const;
    const nlohmann::json& getBalanceData() const;

    // Hot-reload (checks file mtime, reloads if changed)
    bool checkAndReload();

    // Register reload callback
    void onReload(std::function<void()> callback);

private:
    std::unordered_map<std::string, nlohmann::json> content_;
    std::string contentDir_;
    std::vector<std::function<void()>> reloadCallbacks_;
    // File modification times for hot-reload
    std::unordered_map<std::string, int64_t> fileMtimes_;
};

} // namespace vse
```

### 4.10 LocaleManager

```cpp
// include/core/LocaleManager.h
#pragma once
#include "core/Error.h"
#include <nlohmann/json.hpp>
#include <string>

namespace vse {

class LocaleManager {
public:
    Result<bool> loadLocale(const std::string& localeDir, const std::string& lang);
    void setLanguage(const std::string& lang);
    std::string currentLanguage() const;

    // Get translated string. Returns key if not found.
    std::string get(const std::string& key) const;

    // Formatted string: get("welcome", {{"name", "Player"}})
    std::string get(const std::string& key,
                    const std::unordered_map<std::string, std::string>& params) const;

private:
    nlohmann::json strings_;
    std::string currentLang_ = "ko";
};

} // namespace vse
```

### 4.11 IAsyncLoader

```cpp
// include/core/IAsyncLoader.h
#pragma once
#include "core/Error.h"
#include <functional>
#include <string>
#include <vector>

namespace vse {

enum class LoadStatus : uint8_t {
    Pending = 0,
    Loading,
    Complete,
    Failed
};

using LoadCallback = std::function<void(const std::string& path, LoadStatus status)>;

class IAsyncLoader {
public:
    virtual ~IAsyncLoader() = default;

    // Queue a resource for async loading
    virtual void enqueue(const std::string& path, LoadCallback callback) = 0;

    // Process completed loads on main thread (call per frame)
    virtual void processCompleted() = 0;

    // Progress
    virtual int totalQueued() const = 0;
    virtual int totalCompleted() const = 0;
    virtual float progress() const = 0;     // 0.0 - 1.0

    // Check if specific resource is loaded
    virtual LoadStatus getStatus(const std::string& path) const = 0;
};

} // namespace vse
```

### 4.12 IMemoryPool

```cpp
// include/core/IMemoryPool.h
#pragma once
#include <cstddef>

namespace vse {

// Simple block allocator for NPC/tile objects.
// Reduces heap fragmentation for frequent alloc/dealloc patterns.
class IMemoryPool {
public:
    virtual ~IMemoryPool() = default;

    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr) = 0;
    virtual size_t totalAllocated() const = 0;
    virtual size_t totalCapacity() const = 0;
    virtual void reset() = 0;       // Free all (use at scene change)
};

} // namespace vse
```

### 4.13 IRenderCommand (Layer 3 Interface)

```cpp
// include/core/IRenderCommand.h
#pragma once
#include "core/Types.h"
#include <string>
#include <vector>

namespace vse {

// Render commands emitted by domain layer, consumed by renderer.
// Domain never touches SDL2 directly. This is the boundary.
struct RenderTileCmd {
    TileCoord pos;
    TenantType type;
    int spriteId;
};

struct RenderAgentCmd {
    EntityId id;
    PixelPos pos;
    Direction facing;
    AgentState state;
    int spriteFrame;            // Animation frame
};

struct RenderElevatorCmd {
    EntityId id;
    int shaftX;
    int currentFloor;
    float interpolatedY;        // Smooth movement between floors
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

// Collected per frame by Bootstrapper/main loop, passed to renderer
struct RenderFrame {
    std::vector<RenderTileCmd> tiles;
    std::vector<RenderAgentCmd> agents;
    std::vector<RenderElevatorCmd> elevators;
    RenderUICmd ui;
    bool isPaused;
};

} // namespace vse
```

### 4.14 Bootstrapper

```cpp
// include/core/Bootstrapper.h
#pragma once
#include <memory>

namespace vse {

// Forward declarations
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

// Bootstrapper: owns all systems, manages init order, runs main loop.
class Bootstrapper {
public:
    Bootstrapper();
    ~Bootstrapper();

    // Initialize all systems in correct order. Returns false on fatal error.
    bool initialize(const std::string& configPath);

    // Main loop (blocking). Returns exit code.
    int run();

    // Shutdown all systems in reverse order.
    void shutdown();

private:
    // Owned systems (init order = declaration order)
    std::unique_ptr<EventBus> eventBus_;
    std::unique_ptr<ConfigManager> config_;
    std::unique_ptr<ContentRegistry> content_;
    std::unique_ptr<LocaleManager> locale_;
    std::unique_ptr<SimClock> simClock_;
    std::unique_ptr<IGridSystem> gridSystem_;
    std::unique_ptr<IAgentSystem> agentSystem_;
    std::unique_ptr<ITransportSystem> transportSystem_;
    std::unique_ptr<IEconomyEngine> economyEngine_;
    std::unique_ptr<ISaveLoad> saveLoad_;

    // ECS registry
    // entt::registry registry_;   // Defined in cpp

    // SDL2 / Renderer (Layer 3)
    // SDLRenderer renderer_;      // Defined in cpp

    // Main loop internals
    void updateSim(double deltaMsReal);
    void render();
    void handleInput();
    bool running_ = false;
};

} // namespace vse
```

---

## 5. ECS Component & System Map

### 5.1 Components (all data-only)

| Component | Fields | Used By |
|---|---|---|
| `PositionComponent` | tile, pixel, facing | AgentSystem, Renderer |
| `AgentComponent` | state, workplace, homeTenant, satisfaction, moveSpeed | AgentSystem, Economy |
| `AgentScheduleComponent` | workStartHour, workEndHour, lunchHour | AgentSystem |
| `AgentPathComponent` | path[], currentIndex, destination | AgentSystem |
| `ElevatorPassengerComponent` | targetFloor, waiting | TransportSystem |
| `TenantComponent` | type, tilePos, width, rentAmount, maintenanceCost, occupantCount | EconomyEngine, GridSystem |
| `ElevatorComponent` | shaftX, bottomFloor, topFloor, capacity, currentFloor, direction, callQueue | TransportSystem |
| `StarRatingComponent` (singleton) | currentRating, avgSatisfaction, totalPopulation | StarRatingSystem |
| `BuildingComponent` (singleton) | name, builtFloors, totalTenants | GridSystem |

### 5.2 Additional Components

```cpp
// Tenant entity component
struct TenantComponent {
    TenantType type;
    TileCoord position;
    int width;                      // Tiles wide
    int rentAmount;                 // Per day
    int maintenanceCost;            // Per day
    int occupantCount;
    int maxOccupants;
};

// Elevator entity component (stored on elevator entity in registry)
struct ElevatorComponent {
    int shaftX;
    int bottomFloor;
    int topFloor;
    int capacity;
    int currentFloor;
    float interpolatedFloor;        // For smooth rendering
    ElevatorDirection direction = ElevatorDirection::Idle;
    std::vector<int> callQueue;
    std::vector<EntityId> passengers;
};
```

### 5.3 System Update Order (per tick)

```
1. SimClock::update()           — Advance tick, emit TickAdvanced
2. EventBus::flush()            — Process events from previous tick
3. AgentSystem::update()        — NPC AI decisions, pathfinding, movement
4. TransportSystem::update()    — Elevator LOOK algorithm, passenger boarding
5. EconomyEngine::update()      — Rent/maintenance on day boundaries
6. StarRatingSystem::update()   — Recalculate satisfaction average
7. ContentRegistry::checkAndReload()  — Hot-reload (every N ticks)
8. [Render] Collect RenderFrame → SDLRenderer::render()
```

---

## 6. Module Dependency Map

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
     ┌───────────────┐
     │  SDLRenderer   │  Layer 3 (SDL2 + ImGui)
     └───────────────┘
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
- **SDLRenderer** → RenderFrame only (NO domain system access)

---

## 7. Initialization Order (Bootstrapper)

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
10. StarRatingSystem — Needs EventBus
11. SaveLoad         — Needs all above (serializes everything)
12. SDLRenderer      — Needs ConfigManager (window size). SDL_Init here.
13. DebugPanel       — Needs SDLRenderer (ImGui context)

Shutdown: reverse order (13 → 1)
```

---

## 8. Data Flow Diagrams

### 8.1 Main Loop (per frame)

```
┌─ Frame Start ─────────────────────────────────────┐
│                                                     │
│  1. handleInput()                                   │
│     └─ SDL_PollEvent → key/mouse → game commands    │
│                                                     │
│  2. updateSim(realDeltaMs)                         │
│     ├─ SimClock::update(realDeltaMs)               │
│     │   └─ if accumulated >= 100ms → advanceTick() │
│     │       └─ EventBus.publish(TickAdvanced)      │
│     ├─ EventBus::flush()                           │
│     ├─ AgentSystem::update(registry, gameTime, dt) │
│     ├─ TransportSystem::update(dt)                 │
│     ├─ EconomyEngine::update(gameTime)             │
│     └─ StarRatingSystem::update()                  │
│                                                     │
│  3. render()                                        │
│     ├─ Collect RenderFrame from all systems         │
│     ├─ SDLRenderer::render(frame)                  │
│     │   ├─ TileRenderer::draw(frame.tiles)         │
│     │   ├─ AgentRenderer::draw(frame.agents)       │
│     │   ├─ UIRenderer::draw(frame.ui)              │
│     │   └─ DebugPanel::draw() [if debug mode]      │
│     └─ SDL_RenderPresent()                          │
│                                                     │
└─ Frame End ───────────────────────────────────────┘
```

### 8.2 NPC Daily Cycle

```
Spawn (morning)
  │
  ▼
Idle at home floor
  │ workStartHour reached
  ▼
Moving → find path to office
  │ arrived
  ▼
Working (state = Working)
  │ lunchHour reached
  ▼
Moving → find path to commercial floor (or lobby)
  │ arrived
  ▼
Resting (state = Resting)
  │ lunch over
  ▼
Moving → back to office
  │ arrived
  ▼
Working
  │ workEndHour reached
  ▼
Moving → path to home
  │ arrived
  ▼
Despawn (evening)
```

### 8.3 Elevator Flow (LOOK Algorithm)

```
Agent calls elevator
  │
  ▼
EventBus: ElevatorCalled(floor, direction)
  │
  ▼
TransportSystem adds floor to callQueue
  │
  ▼
LOOK: continue current direction, stop at called floors
  │ arrived at agent's floor
  ▼
EventBus: ElevatorArrived(floor)
  │
  ▼
Agent boards → AgentState = InElevator
  │
  ▼
Elevator continues to target floor
  │ arrived
  ▼
Agent exits → AgentState = Moving (to destination)
  │
  ▼
EventBus: ElevatorExited
```

---

## 9. JSON Config Schemas

### 9.1 game_config.json

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

### 9.2 balance.json (hot-reloadable)

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

### 9.3 tenants.json (Content Pack)

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

## 10. Phase 3 Stub Interfaces (Empty)

```cpp
// include/core/INetworkAdapter.h
#pragma once
namespace vse {
// [PHASE-3] Network adapter for authoritative server model.
// Will implement: connect, disconnect, sendState, receiveState, sync.
class INetworkAdapter {
public:
    virtual ~INetworkAdapter() = default;
    // Methods TBD in Phase 3 design.
};
} // namespace vse

// include/core/IAuthority.h
#pragma once
namespace vse {
// [PHASE-3] Authority for server-side state validation.
// Will implement: validateAction, resolveConflict, replicateState.
class IAuthority {
public:
    virtual ~IAuthority() = default;
    // Methods TBD in Phase 3 design.
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
    // Methods TBD after performance measurement.
};
} // namespace vse
```

---

## 11. Layer Boundary Enforcement Checklist

| Rule | How to Verify |
|---|---|
| Layer 3 has no game logic | SDLRenderer.cpp has zero `if(satisfaction > X)` or `if(balance < 0)` |
| Layer 0 has no concrete impl | All files in `include/core/I*.h` are pure virtual or data structs |
| Layer 1 has no SDL2 calls | `grep -r "SDL_" src/domain/` returns nothing |
| Layer 1 has no `#include <SDL` | `grep -r "SDL" include/domain/` returns nothing |
| Domain produces RenderFrame only | Domain systems return data, never call render functions |
| ConfigManager is the only JSON reader | No `nlohmann::json` in domain code (except via ConfigManager/ContentRegistry) |

---

## Change Log

| Date | Version | Change |
|---|---|---|
| 2026-03-24 | 1.0 | Initial design spec. All Layer 0 interfaces, ECS components, dependency map, init order, JSON schemas, data flow. |

---
*This document is maintained by 붐 (PM). Changes require Human approval.*
*Implementation must follow CLAUDE.md constraints. This doc provides the HOW.*
