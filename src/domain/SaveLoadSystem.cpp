#include "domain/SaveLoadSystem.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace vse {

SaveLoadSystem::SaveLoadSystem(entt::registry& reg,
                               SimClock& clock,
                               GridSystem& grid,
                               EconomyEngine& economy,
                               StarRatingSystem& starRating,
                               TransportSystem& transport,
                               AgentSystem& agents,
                               const std::string& saveDir)
    : reg_(reg), clock_(clock), grid_(grid), economy_(economy)
    , starRating_(starRating), transport_(transport), agents_(agents)
    , saveDir_(saveDir)
{
    if (!fs::exists(saveDir_)) {
        fs::create_directories(saveDir_);
    }
}

// ── Save ────────────────────────────────────────────────────────────────────

Result<bool> SaveLoadSystem::save(const std::string& filepath) {
    json root;
    root["metadata"]   = serializeMetadata();
    root["simclock"]   = serializeSimClock();
    root["grid"]       = grid_.exportState();
    root["entities"]   = serializeEntities();
    root["economy"]    = economy_.exportState();
    root["starrating"] = starRating_.exportState(reg_);
    root["transport"]  = transport_.exportState();

    auto msgpack = json::to_msgpack(root);

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) {
        spdlog::warn("SaveLoadSystem::save: cannot open file {}", filepath);
        return Result<bool>::failure(ErrorCode::SystemError);
    }
    out.write(reinterpret_cast<const char*>(msgpack.data()),
              static_cast<std::streamsize>(msgpack.size()));
    out.close();

    spdlog::info("SaveLoadSystem::save: saved to {} ({} bytes)", filepath, msgpack.size());
    return Result<bool>::success(true);
}

// ── Load ────────────────────────────────────────────────────────────────────

Result<bool> SaveLoadSystem::load(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        spdlog::warn("SaveLoadSystem::load: cannot open file {}", filepath);
        return Result<bool>::failure(ErrorCode::SystemError);
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    in.close();

    if (data.empty()) {
        spdlog::warn("SaveLoadSystem::load: empty file");
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }

    json root = json::from_msgpack(data, true, false);
    if (root.is_discarded()) {
        spdlog::warn("SaveLoadSystem::load: invalid msgpack data");
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }

    // 1. Verify metadata version
    if (!root.contains("metadata") || root["metadata"].value("version", 0u) != SAVE_VERSION) {
        spdlog::warn("SaveLoadSystem::load: version mismatch");
        return Result<bool>::failure(ErrorCode::InvalidArgument);
    }

    // 2. Clear registry + agent tracking
    reg_.clear();
    agents_.clearTracking();

    // 3. Restore Grid
    if (root.contains("grid")) {
        grid_.importState(root["grid"]);
    }

    // 4-7. Restore ECS entities (returns remap table for cross-ref fixups)
    std::unordered_map<uint32_t, EntityId> remap;
    if (root.contains("entities")) {
        remap = deserializeEntities(root["entities"]);
    }

    // 7.5. Remap Grid tenantEntity references (P0 fix)
    grid_.remapEntityIds(remap);

    // 8. Restore Economy
    if (root.contains("economy")) {
        economy_.importState(root["economy"]);
    }

    // 9. Restore StarRating
    if (root.contains("starrating")) {
        starRating_.importState(reg_, root["starrating"]);
    }

    // 10. Restore Transport (with entity remap for passengers)
    if (root.contains("transport")) {
        transport_.importState(root["transport"]);
        transport_.remapPassengerIds(remap);
    }

    // 11. Restore SimClock
    if (root.contains("simclock")) {
        auto& sc = root["simclock"];
        clock_.restoreState(
            sc.value("tick", SimTick(0)),
            sc.value("speed", 1),
            sc.value("paused", false)
        );
    }

    spdlog::info("SaveLoadSystem::load: loaded from {} successfully", filepath);
    return Result<bool>::success(true);
}

// ── Quick save/load ─────────────────────────────────────────────────────────

Result<bool> SaveLoadSystem::quickSave() {
    fs::path p = fs::path(saveDir_) / QUICKSAVE_NAME;
    return save(p.string());
}

Result<bool> SaveLoadSystem::quickLoad() {
    fs::path p = fs::path(saveDir_) / QUICKSAVE_NAME;
    return load(p.string());
}

// ── Save path generation ────────────────────────────────────────────────────

std::string SaveLoadSystem::getSavePath(int slotIndex) const {
    if (slotIndex == 0) {
        // Slot 0 is auto-save
        fs::path p = fs::path(saveDir_) / "autosave.vsesave";
        return p.string();
    } else {
        // Manual save slots 1-4
        fs::path p = fs::path(saveDir_) / ("save" + std::to_string(slotIndex) + ".vsesave");
        return p.string();
    }
}

// ── Metadata ────────────────────────────────────────────────────────────────

Result<SaveMetadata> SaveLoadSystem::readMetadata(const std::string& filepath) const {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        return Result<SaveMetadata>::failure(ErrorCode::SystemError);
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    in.close();

    json root = json::from_msgpack(data, true, false);
    if (root.is_discarded() || !root.contains("metadata")) {
        return Result<SaveMetadata>::failure(ErrorCode::InvalidArgument);
    }

    auto& m = root["metadata"];
    SaveMetadata meta;
    meta.version         = m.value("version", 0u);
    meta.buildingName    = m.value("buildingName", std::string(""));
    meta.gameTime.day    = m.value("day", 0);
    meta.gameTime.hour   = m.value("hour", 0);
    meta.gameTime.minute = m.value("minute", 0);
    meta.starRating      = m.value("starRating", 1);
    meta.balance         = m.value("balance", int64_t(0));
    meta.playtimeSeconds = m.value("playtimeSeconds", uint64_t(0));

    return Result<SaveMetadata>::success(meta);
}

std::vector<std::string> SaveLoadSystem::listSaves() const {
    std::vector<std::string> result;
    if (!fs::exists(saveDir_)) return result;
    for (auto& entry : fs::directory_iterator(saveDir_)) {
        if (entry.path().extension() == ".vsesave") {
            result.push_back(entry.path().string());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

// ── Serialize helpers ───────────────────────────────────────────────────────

json SaveLoadSystem::serializeMetadata() const {
    auto time = clock_.currentGameTime();
    json j;
    j["version"]         = SAVE_VERSION;
    j["buildingName"]    = "My Tower";
    j["day"]             = time.day;
    j["hour"]            = time.hour;
    j["minute"]          = time.minute;
    j["starRating"]      = static_cast<int>(starRating_.getCurrentRating(reg_));
    j["balance"]         = economy_.getBalance();
    j["playtimeSeconds"] = uint64_t(0);
    return j;
}

json SaveLoadSystem::serializeSimClock() const {
    json j;
    j["tick"]   = clock_.currentTick();
    j["speed"]  = clock_.speed();
    j["paused"] = clock_.isPaused();
    return j;
}

json SaveLoadSystem::serializeEntities() const {
    json entities = json::array();

    // Serialize agents
    auto agentView = reg_.view<AgentComponent, PositionComponent, AgentScheduleComponent>();
    for (auto entity : agentView) {
        const auto& agent = agentView.get<AgentComponent>(entity);
        const auto& pos   = agentView.get<PositionComponent>(entity);
        const auto& sched = agentView.get<AgentScheduleComponent>(entity);

        json e;
        e["id"]   = static_cast<uint32_t>(entity);
        e["type"] = "agent";

        json aj;
        aj["state"]           = static_cast<int>(agent.state);
        aj["homeTenant"]      = static_cast<uint32_t>(agent.homeTenant);
        aj["workplaceTenant"] = static_cast<uint32_t>(agent.workplaceTenant);
        aj["satisfaction"]    = agent.satisfaction;
        aj["moveSpeed"]       = agent.moveSpeed;
        aj["stress"]          = agent.stress;
        aj["stairTargetFloor"] = agent.stairTargetFloor;
        aj["stairTicksRemaining"] = agent.stairTicksRemaining;
        aj["elevatorWaitTicks"] = agent.elevatorWaitTicks;
        e["agent"] = aj;

        json pj;
        pj["tileX"]     = pos.tile.x;
        pj["tileFloor"] = pos.tile.floor;
        pj["pixelX"]    = pos.pixel.x;
        pj["pixelY"]    = pos.pixel.y;
        pj["facing"]    = static_cast<int>(pos.facing);
        e["position"] = pj;

        json sj;
        sj["workStartHour"] = sched.workStartHour;
        sj["workEndHour"]   = sched.workEndHour;
        sj["lunchHour"]     = sched.lunchHour;
        e["schedule"] = sj;

        // Optional: AgentPathComponent
        if (reg_.all_of<AgentPathComponent>(entity)) {
            const auto& path = reg_.get<AgentPathComponent>(entity);
            json pathArr = json::array();
            for (auto& tc : path.path) {
                json tcj;
                tcj["x"] = tc.x;
                tcj["floor"] = tc.floor;
                pathArr.push_back(tcj);
            }
            json paj;
            paj["path"]         = pathArr;
            paj["currentIndex"] = path.currentIndex;
            paj["destX"]        = path.destination.x;
            paj["destFloor"]    = path.destination.floor;
            e["path"] = paj;
        }

        // Optional: ElevatorPassengerComponent
        if (reg_.all_of<ElevatorPassengerComponent>(entity)) {
            const auto& ep = reg_.get<ElevatorPassengerComponent>(entity);
            json epj;
            epj["targetFloor"] = ep.targetFloor;
            epj["waiting"]     = ep.waiting;
            e["elevatorPassenger"] = epj;
        }

        entities.push_back(e);
    }

    // Serialize StarRatingComponent singleton
    auto srView = reg_.view<StarRatingComponent>();
    for (auto entity : srView) {
        if (reg_.all_of<AgentComponent>(entity)) continue;

        const auto& sr = srView.get<StarRatingComponent>(entity);
        json e;
        e["id"]   = static_cast<uint32_t>(entity);
        e["type"] = "starrating_singleton";
        json srj;
        srj["currentRating"]    = static_cast<int>(sr.currentRating);
        srj["avgSatisfaction"]  = sr.avgSatisfaction;
        srj["totalPopulation"]  = sr.totalPopulation;
        e["starRatingComponent"] = srj;
        entities.push_back(e);
    }

    // Serialize TenantComponent entities
    auto tenantView = reg_.view<TenantComponent>();
    for (auto entity : tenantView) {
        const auto& tenant = tenantView.get<TenantComponent>(entity);
        
        json e;
        e["id"]   = static_cast<uint32_t>(entity);
        e["type"] = "tenant";
        
        json tj;
        tj["type"]               = static_cast<int>(tenant.type);
        tj["anchorTileX"]        = tenant.anchorTile.x;
        tj["anchorTileFloor"]    = tenant.anchorTile.floor;
        tj["width"]              = tenant.width;
        tj["rentPerDay"]         = tenant.rentPerDay;
        tj["maintenanceCost"]    = tenant.maintenanceCost;
        tj["maxOccupants"]       = tenant.maxOccupants;
        tj["occupantCount"]      = tenant.occupantCount;
        tj["buildCost"]          = tenant.buildCost;
        tj["satisfactionDecayRate"] = tenant.satisfactionDecayRate;
        tj["isEvicted"]          = tenant.isEvicted;
        tj["evictionCountdown"]  = tenant.evictionCountdown;
        e["tenant"] = tj;
        
        entities.push_back(e);
    }

    return entities;
}

// ── Deserialize entities ────────────────────────────────────────────────────

std::unordered_map<uint32_t, EntityId> SaveLoadSystem::deserializeEntities(const json& entities) {
    std::unordered_map<uint32_t, EntityId> remap;

    // Pass 1: Create entities + attach components (raw old IDs)
    for (const auto& e : entities) {
        uint32_t oldId = e.value("id", uint32_t(0));
        std::string type = e.value("type", std::string(""));

        EntityId newEntity = reg_.create();
        remap[oldId] = newEntity;

        if (type == "agent") {
            if (e.contains("agent")) {
                auto& a = e["agent"];
                auto& comp = reg_.emplace<AgentComponent>(newEntity);
                comp.state           = static_cast<AgentState>(a.value("state", 0));
                comp.homeTenant      = static_cast<EntityId>(a.value("homeTenant", uint32_t(0)));
                comp.workplaceTenant = static_cast<EntityId>(a.value("workplaceTenant", uint32_t(0)));
                comp.satisfaction    = a.value("satisfaction", 100.0f);
                comp.moveSpeed       = a.value("moveSpeed", 1.0f);
                comp.stress          = a.value("stress", 0.0f);
                comp.stairTargetFloor = a.value("stairTargetFloor", -1);
                comp.stairTicksRemaining = a.value("stairTicksRemaining", 0);
                comp.elevatorWaitTicks = a.value("elevatorWaitTicks", 0);
            }

            if (e.contains("position")) {
                auto& p = e["position"];
                auto& comp = reg_.emplace<PositionComponent>(newEntity);
                comp.tile.x     = p.value("tileX", 0);
                comp.tile.floor = p.value("tileFloor", 0);
                comp.pixel.x    = p.value("pixelX", 0.0f);
                comp.pixel.y    = p.value("pixelY", 0.0f);
                comp.facing     = static_cast<Direction>(p.value("facing", 0));
            }

            if (e.contains("schedule")) {
                auto& s = e["schedule"];
                auto& comp = reg_.emplace<AgentScheduleComponent>(newEntity);
                comp.workStartHour = s.value("workStartHour", 9);
                comp.workEndHour   = s.value("workEndHour", 18);
                comp.lunchHour     = s.value("lunchHour", 12);
            }

            if (e.contains("path")) {
                auto& pa = e["path"];
                auto& comp = reg_.emplace<AgentPathComponent>(newEntity);
                comp.currentIndex = pa.value("currentIndex", 0);
                comp.destination.x     = pa.value("destX", 0);
                comp.destination.floor = pa.value("destFloor", 0);
                if (pa.contains("path")) {
                    for (auto& tc : pa["path"]) {
                        comp.path.push_back({tc.value("x", 0), tc.value("floor", 0)});
                    }
                }
            }

            if (e.contains("elevatorPassenger")) {
                auto& ep = e["elevatorPassenger"];
                auto& comp = reg_.emplace<ElevatorPassengerComponent>(newEntity);
                comp.targetFloor = ep.value("targetFloor", 0);
                comp.waiting     = ep.value("waiting", true);
            }

            agents_.registerRestoredAgent(newEntity);

        } else if (type == "starrating_singleton") {
            if (e.contains("starRatingComponent")) {
                auto& s = e["starRatingComponent"];
                auto& comp = reg_.emplace<StarRatingComponent>(newEntity);
                comp.currentRating   = static_cast<StarRating>(s.value("currentRating", 1));
                comp.avgSatisfaction = s.value("avgSatisfaction", 50.0f);
                comp.totalPopulation = s.value("totalPopulation", 0);
            }
        } else if (type == "tenant") {
            if (e.contains("tenant")) {
                auto& t = e["tenant"];
                auto& comp = reg_.emplace<TenantComponent>(newEntity);
                comp.type               = static_cast<TenantType>(t.value("type", 0));
                comp.anchorTile.x       = t.value("anchorTileX", 0);
                comp.anchorTile.floor   = t.value("anchorTileFloor", 0);
                comp.width              = t.value("width", 1);
                comp.rentPerDay         = t.value("rentPerDay", 0);
                comp.maintenanceCost    = t.value("maintenanceCost", 0);
                comp.maxOccupants       = t.value("maxOccupants", 0);
                comp.occupantCount      = t.value("occupantCount", 0);
                comp.buildCost          = t.value("buildCost", 0);
                comp.satisfactionDecayRate = t.value("satisfactionDecayRate", 0.0f);
                comp.isEvicted          = t.value("isEvicted", false);
                comp.evictionCountdown  = t.value("evictionCountdown", 0);
            }
        }
    }

    // Pass 2: Remap EntityId cross-references
    auto agentView = reg_.view<AgentComponent>();
    for (auto entity : agentView) {
        auto& agent = agentView.get<AgentComponent>(entity);

        auto homeIt = remap.find(static_cast<uint32_t>(agent.homeTenant));
        if (homeIt != remap.end()) {
            agent.homeTenant = homeIt->second;
        }

        auto workIt = remap.find(static_cast<uint32_t>(agent.workplaceTenant));
        if (workIt != remap.end()) {
            agent.workplaceTenant = workIt->second;
        }
    }

    return remap;
}

} // namespace vse
