#pragma once
#include "core/ISaveLoad.h"
#include "core/SimClock.h"
#include "core/EventBus.h"
#include "domain/GridSystem.h"
#include "domain/EconomyEngine.h"
#include "domain/StarRatingSystem.h"
#include "domain/TransportSystem.h"
#include "domain/AgentSystem.h"
#include <entt/entt.hpp>
#include <string>

namespace vse {

/**
 * SaveLoadSystem — Phase 1 save/load implementation.
 *
 * Serialization format: MessagePack (via nlohmann::json::to_msgpack).
 * Layout: JSON object → MessagePack binary.
 *
 * Save pipeline:
 *   1. Build JSON: metadata + SimClock + Grid + ECS entities + Economy + StarRating + Transport
 *   2. nlohmann::json::to_msgpack() → binary
 *   3. Write to file
 *
 * Load pipeline:
 *   1. Read file → nlohmann::json::from_msgpack()
 *   2. Verify metadata version
 *   3. Clear registry
 *   4. Restore Grid (floors + tiles)
 *   5. Restore ECS entities (tenants, elevators, agents — with components)
 *   6. Restore Economy state
 *   7. Restore StarRating state
 *   8. Restore Transport state (elevator cars, FSM, passengers)
 *   9. Restore SimClock (tick, speed, paused)
 *   10. Recalculate derived caches
 *
 * Entity ID strategy:
 *   Save: store raw uint32_t entity IDs + component data.
 *   Load: create entities in same order, build oldId → newId remap table.
 *   All cross-references (homeTenant, workplaceTenant, passengers) are remapped.
 */
class SaveLoadSystem : public ISaveLoad {
public:
    SaveLoadSystem(entt::registry& reg,
                   SimClock& clock,
                   GridSystem& grid,
                   EconomyEngine& economy,
                   StarRatingSystem& starRating,
                   TransportSystem& transport,
                   AgentSystem& agents,
                   const std::string& saveDir = "saves");

    Result<bool> save(const std::string& filepath) override;
    Result<bool> load(const std::string& filepath) override;
    Result<bool> quickSave() override;
    Result<bool> quickLoad() override;
    Result<SaveMetadata> readMetadata(const std::string& filepath) const override;
    std::vector<std::string> listSaves() const override;

private:
    entt::registry&   reg_;
    SimClock&         clock_;
    GridSystem&       grid_;
    EconomyEngine&    economy_;
    StarRatingSystem& starRating_;
    TransportSystem&  transport_;
    AgentSystem&      agents_;
    std::string       saveDir_;

    static constexpr uint32_t SAVE_VERSION = 1;
    static constexpr const char* QUICKSAVE_NAME = "quicksave.vsesave";

    // ── Serialization helpers ──────────────────────────────────────────────
    nlohmann::json serializeMetadata() const;
    nlohmann::json serializeSimClock() const;
    nlohmann::json serializeEntities() const;

    // ── Deserialization helpers ─────────────────────────────────────────────
    // Returns oldId → newId remap table for post-load fixups
    std::unordered_map<uint32_t, EntityId> deserializeEntities(const nlohmann::json& j);
};

} // namespace vse
