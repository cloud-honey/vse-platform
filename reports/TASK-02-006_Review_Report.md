# VSE Platform — TASK-02-006 Review Report

> Author: claude-opus-4-6
> Date: 2026-03-25
> Task: TASK-02-006 ISaveLoad + SaveLoad (MessagePack Serialization)
> Layer: Layer 1 — Domain Module
> Tests: 14 new test cases + 187 existing = **201/201 passed**
> Commit: 97f8df4 (post-review fix)

---

## Files

| File | Type |
|---|---|
| `include/core/ISaveLoad.h` | New — ISaveLoad interface + SaveMetadata struct |
| `include/domain/SaveLoadSystem.h` | New — SaveLoadSystem class declaration |
| `src/domain/SaveLoadSystem.cpp` | New — Full save/load implementation |
| `include/domain/GridSystem.h` | Modified — added exportState()/importState() |
| `src/domain/GridSystem.cpp` | Modified — export/import implementation |
| `include/domain/EconomyEngine.h` | Modified — added exportState()/importState() |
| `src/domain/EconomyEngine.cpp` | Modified — export/import implementation |
| `include/domain/StarRatingSystem.h` | Modified — added exportState()/importState() |
| `src/domain/StarRatingSystem.cpp` | Modified — export/import implementation |
| `include/domain/TransportSystem.h` | Modified — added exportState()/importState() |
| `src/domain/TransportSystem.cpp` | Modified — export/import implementation |
| `include/domain/AgentSystem.h` | Modified — added registerRestoredAgent()/clearTracking() |
| `src/domain/AgentSystem.cpp` | Modified — registerRestoredAgent/clearTracking implementation |
| `tests/test_SaveLoad.cpp` | New — 13 test cases |
| `tests/CMakeLists.txt` | Modified — added test_SaveLoad.cpp + SaveLoadSystem.cpp |
| `CMakeLists.txt` | Modified — added SaveLoadSystem.cpp to TowerTycoon target |

---

## Interface

### ISaveLoad (new)
```cpp
struct SaveMetadata {
    uint32_t    version = 1;
    std::string buildingName;
    GameTime    gameTime;
    int         starRating = 1;
    int64_t     balance    = 0;
    uint64_t    playtimeSeconds = 0;
};

class ISaveLoad {
    virtual Result<bool> save(const std::string& filepath) = 0;
    virtual Result<bool> load(const std::string& filepath) = 0;
    virtual Result<bool> quickSave() = 0;
    virtual Result<bool> quickLoad() = 0;
    virtual Result<SaveMetadata> readMetadata(const std::string& filepath) const = 0;
    virtual std::vector<std::string> listSaves() const = 0;
};
```

### SaveLoadSystem constructor
```cpp
SaveLoadSystem(entt::registry& reg, SimClock& clock, GridSystem& grid,
               EconomyEngine& economy, StarRatingSystem& starRating,
               TransportSystem& transport, AgentSystem& agents,
               const std::string& saveDir = "saves");
```

### System export/import methods (added to each domain class)
```cpp
// GridSystem
nlohmann::json exportState() const;
void importState(const nlohmann::json& j);

// EconomyEngine
nlohmann::json exportState() const;
void importState(const nlohmann::json& j);

// StarRatingSystem
nlohmann::json exportState(const entt::registry& reg) const;
void importState(entt::registry& reg, const nlohmann::json& j);

// TransportSystem
nlohmann::json exportState() const;
void importState(const nlohmann::json& j);

// AgentSystem
void registerRestoredAgent(EntityId id);
void clearTracking();
```

---

## Key Behaviors

### Serialization format
- **JSON → MessagePack binary** via `nlohmann::json::to_msgpack()` / `from_msgpack()`.
- No external MessagePack library needed — nlohmann/json has built-in support.
- File extension: `.vsesave`.

### Save pipeline
1. Build JSON object: metadata + simclock + grid + entities + economy + starrating + transport
2. `nlohmann::json::to_msgpack()` → binary vector
3. Write binary to file

### Load pipeline (11-step restore order per Design Spec)
1. Read file → `nlohmann::json::from_msgpack()`
2. Verify metadata version (reject if mismatch)
3. Clear `entt::registry` + `AgentSystem::clearTracking()`
4. Restore Grid (floors + tile occupancy)
5. Restore ECS entities — two-pass approach:
   - Pass 1: Create entities, attach components with raw old IDs
   - Pass 2: Remap cross-references (homeTenant, workplaceTenant) using oldId→newId table
6. Restore Economy state (balance, income/expense history)
7. Restore StarRating state (singleton component)
8. Restore Transport state (elevator cars, FSM, passengers, call queues)
9. Restore SimClock (tick, speed, paused) via `restoreState()` (silent — no events)
10. (Phase 1: derived cache recalculation deferred — agent paths not actively used for horizontal movement yet)

### Entity ID strategy (Phase 1 — no EnTT snapshot)
- **Decision:** Manual serialization instead of `entt::snapshot`. Rationale: only 5 ECS component types, `entt::snapshot` requires a custom archive adapter for MessagePack that adds complexity without benefit at this scale.
- Save: store `static_cast<uint32_t>(entity)` for each entity
- Load: create entities in sequence, build `unordered_map<uint32_t, EntityId>` remap table
- Cross-references (`AgentComponent::homeTenant`, `AgentComponent::workplaceTenant`) remapped in Pass 2
- Grid `TileData::tenantEntity` IDs are NOT remapped (grid stores raw saved IDs — these are only used for anchor lookup, not ECS queries). This is a Phase 1 simplification.

### Non-ECS state serialization
Each system exports/imports its own internal state:
- **GridSystem:** `floors_` map (floor number → built status + all TileData)
- **EconomyEngine:** balance, dailyIncome/Expense, lastRentDay/MaintenanceDay, income/expense history
- **StarRatingSystem:** singleton StarRatingComponent values
- **TransportSystem:** all ElevatorCar structs (FSM state, passengers, carCalls, hallCalls, waitingCount)

---

## Test Cases (13)

| # | Test Name | Result |
|---|---|---|
| 1 | Basic save and load round-trip | ✅ Pass |
| 2 | SimClock state preserved (tick, speed=2, paused) | ✅ Pass |
| 3 | Grid floors and tiles preserved (3 floors, elevator shaft, tenant tiles) | ✅ Pass |
| 4 | Economy balance preserved (addIncome + addExpense → round-trip) | ✅ Pass |
| 5 | Agent components preserved (state, satisfaction, position) | ✅ Pass |
| 6 | homeTenant/workplaceTenant references survive round-trip | ✅ Pass |
| 7 | readMetadata without full load | ✅ Pass |
| 8 | quickSave and quickLoad | ✅ Pass |
| 9 | Multiple agents saved and restored (2 agents) | ✅ Pass |
| 10 | Transport elevator state preserved | ✅ Pass |
| 11 | Load non-existent file returns failure | ✅ Pass |
| 12 | listSaves returns .vsesave files | ✅ Pass |
| 13 | Version mismatch rejects load | ✅ Pass |

---

## Deviations from Spec

1. **Manual serialization instead of EnTT snapshot**: Design Spec specifies `entt::snapshot` / `entt::snapshot_loader`. Implementation uses manual view iteration + JSON serialization. Rationale: 5 component types, custom archive adapter overhead not justified. EnTT's automatic ID remapping is replicated by the two-pass remap table approach.

2. **Grid tenant EntityId NOT remapped**: Grid tiles store saved EntityIds as-is. Since Grid tiles are imported before ECS entities, and Grid only uses tenantEntity for anchor lookup (not ECS queries), this works for Phase 1. Phase 2 should add Grid tile entity remapping for full correctness.

3. **No exception handling**: Project uses `-fno-exceptions`. SaveLoadSystem uses `json::from_msgpack(data, true, false)` with `allow_exceptions=false` and checks `is_discarded()` instead of try/catch.

4. **Playtime tracking not implemented**: `SaveMetadata::playtimeSeconds` always saves 0. Phase 2 will add real-time tracking.

5. **TransportSystem passenger EntityIds NOT remapped**: Elevator car passengers are stored as raw saved IDs. Since these reference Agent entities in the main registry, they should be remapped. Phase 1 limitation — Transport tests verify car count, not individual passenger identity.

---

## Troubleshooting

### AgentSystem activeAgents_ not cleared on load
- **Symptom**: After load, `activeAgentCount()` returned double the expected count (original + restored)
- **Root cause**: `reg_.clear()` destroys entities but `AgentSystem::activeAgents_` is a separate `unordered_set` not linked to the registry
- **Fix**: Added `AgentSystem::clearTracking()` method, called during load before entity restoration

### SimClock setSpeed assert
- **Symptom**: `setSpeed(3)` in test caused SIGABRT — speed only allows 1, 2, 4
- **Fix**: Changed test to use `setSpeed(2)`

### No-exception build compatibility
- **Symptom**: Initial implementation used try/catch → compile error with `-fno-exceptions`
- **Fix**: Rewrote to use `json::from_msgpack(data, true, false)` (no-throw variant) + `is_discarded()` check

---

## Open Items

- ~~**P1**: Grid `TileData::tenantEntity` IDs not remapped~~ → **Fixed** (commit 97f8df4). `GridSystem::remapEntityIds()` added.
- ~~**P1**: TransportSystem `ElevatorCar::passengers` EntityIds not remapped~~ → **Fixed** (commit 97f8df4). `TransportSystem::remapPassengerIds()` added. Test 14 verifies passenger identity after round-trip.
- **P2**: `SaveMetadata::playtimeSeconds` always 0. Add real-time tracking in Bootstrapper.
- **P2**: `buildingName` hardcoded to "My Tower". Add user input or config-based naming.
- **P2**: Consider `entt::snapshot` for Phase 2 if component count grows significantly.

---

## Cross-Validation

| Model | Verdict | Key Issues |
|---|---|---|
| GPT-5.4 Thinking | Conditional Pass → **Pass (Post-Fix)** | P0: Grid tenantEntity remap 누락(→수정완료), Transport passenger remap 누락(→수정완료, 테스트 14 추가); P1: SaveMetadata balance int64_t 타입 설계문서 불일치(→보고서 기록), entt::snapshot deviation 서술 정정 |
| DeepSeek V3 | Conditional Pass → **Pass (Post-Fix)** | P1: Entity ID remap 미완(→GPT P0으로 이미 수정완료), EnTT snapshot 스펙 이탈(→Design Spec §5.8 업데이트 완료); P2: playtimeSeconds=0, buildingName 하드코딩 |

---

## Review Request

Please review this report against `CLAUDE.md` and `VSE_Design_Spec.md`.

**Instructions for the reviewing model:**
1. Use the cross-validation review template below.
2. Include **your model name** in the `Reviewer` field.
3. List **only issues found** — omit anything that passed.
4. Deliver the result as a **Markdown (.md) file**.

**Review template:**

```markdown
# Cross-Validation Review — TASK-02-006

> Reviewer: [YOUR MODEL NAME HERE]
> Verdict: [Pass / Conditional Pass / Fail]

## Issues Found

### P0 — Must Fix Before Next Task
(none, or describe issue)

### P1 — Fix Soon
(none, or describe issue)

### P2 — Nice to Have
(none, or describe issue)

---
*If no issues: write "No issues found." and stop.*
```
