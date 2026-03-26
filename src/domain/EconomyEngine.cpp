/**
 * @file EconomyEngine.cpp
 * @layer Layer 1 — Domain Module
 * @task TASK-02-003
 * @author DeepSeek V3
 * @reviewed pending
 *
 * @brief EconomyEngine implementation.
 */

#include "domain/EconomyEngine.h"
#include "core/EventBus.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstdint>
#include <climits>

namespace vse {

EconomyEngine::EconomyEngine(const EconomyConfig& config, EventBus& eventBus)
    : balance_(config.startingBalance)
    , config_(config)
    , eventBus_(eventBus) {
    spdlog::debug("EconomyEngine created with starting balance: {} cents", balance_);
}

int64_t EconomyEngine::getBalance() const {
    return balance_;
}

void EconomyEngine::addIncome(EntityId tenant, TenantType type, int64_t amount, const GameTime& time) {
    if (amount <= 0) {
        spdlog::warn("EconomyEngine::addIncome: non-positive amount {} cents ignored", amount);
        return;
    }
    
    // Overflow guard
    if (balance_ > INT64_MAX - amount) {
        spdlog::warn("EconomyEngine::addIncome: overflow detected, clamping to INT64_MAX");
        balance_ = INT64_MAX;
    } else {
        balance_ += amount;
    }
    
    dailyIncome_ += amount;
    
    IncomeRecord record{tenant, type, amount, time};
    addIncomeRecord(record);
    
    spdlog::debug("EconomyEngine: added income {} cents from tenant {}, new balance: {} cents", 
                  amount, static_cast<uint32_t>(tenant), balance_);
}

void EconomyEngine::addExpense(const std::string& category, int64_t amount, const GameTime& time) {
    if (amount <= 0) {
        spdlog::warn("EconomyEngine::addExpense: non-positive amount {} cents ignored", amount);
        return;
    }
    
    // Underflow guard — balance can go negative (no debt/interest mechanics in Phase 1,
    // but clamping to INT64_MIN prevents undefined behavior from overflow)
    if (balance_ < INT64_MIN + amount) {
        spdlog::warn("EconomyEngine::addExpense: underflow detected, clamping to INT64_MIN");
        balance_ = INT64_MIN;
    } else {
        balance_ -= amount;
    }
    
    dailyExpense_ += amount;
    
    ExpenseRecord record{category, amount, time};
    addExpenseRecord(record);
    
    spdlog::debug("EconomyEngine: added expense {} cents for {}, new balance: {} cents", 
                  amount, category, balance_);
}

void EconomyEngine::collectRent(const IGridSystem& grid, const GameTime& time) {
    // Guard: only collect once per game day
    if (time.day == lastRentDay_) {
        return;
    }
    
    spdlog::debug("EconomyEngine: collecting rent for day {}", time.day);
    
    // Iterate through all floors
    for (int floor = 0; floor < grid.maxFloors(); ++floor) {
        if (!grid.isFloorBuilt(floor)) {
            continue;
        }
        
        // Get all tiles on this floor
        auto tiles = grid.getFloorTiles(floor);
        for (const auto& [coord, tileData] : tiles) {
            // Only process anchor tiles to avoid double-counting
            if (tileData.isAnchor && tileData.tenantEntity != INVALID_ENTITY) {
                int64_t rentPerDay = 0;
                
                switch (tileData.tenantType) {
                    case TenantType::Office:
                        rentPerDay = config_.officeRentPerTilePerDay * tileData.tileWidth;
                        break;
                    case TenantType::Residential:
                        rentPerDay = config_.residentialRentPerTilePerDay * tileData.tileWidth;
                        break;
                    case TenantType::Commercial:
                        rentPerDay = config_.commercialRentPerTilePerDay * tileData.tileWidth;
                        break;
                    default:
                        // COUNT or invalid type
                        continue;
                }
                
                if (rentPerDay > 0) {
                    addIncome(tileData.tenantEntity, tileData.tenantType, rentPerDay, time);
                }
            }
        }
    }
    
    lastRentDay_ = time.day;
}

void EconomyEngine::payMaintenance(const IGridSystem& grid, const GameTime& time) {
    // Guard: only pay once per game day
    if (time.day == lastMaintenanceDay_) {
        return;
    }
    
    // Count elevator shafts
    int elevatorShaftCount = 0;
    for (int floor = 0; floor < grid.maxFloors(); ++floor) {
        if (!grid.isFloorBuilt(floor)) {
            continue;
        }
        
        // Check each tile on the floor
        for (int x = 0; x < grid.floorWidth(); ++x) {
            TileCoord coord{x, floor};
            if (grid.isElevatorShaft(coord)) {
                // Check if this is an anchor tile (to avoid double-counting)
                auto tileData = grid.getTile(coord);
                if (tileData && tileData->isAnchor) {
                    elevatorShaftCount++;
                }
            }
        }
    }
    
    if (elevatorShaftCount > 0) {
        int64_t maintenanceCost = config_.elevatorMaintenancePerDay * elevatorShaftCount;
        addExpense("elevator", maintenanceCost, time);
        spdlog::debug("EconomyEngine: paid {} cents for {} elevator shafts", 
                      maintenanceCost, elevatorShaftCount);
    }
    
    lastMaintenanceDay_ = time.day;
}

void EconomyEngine::update(const GameTime& time) {
    // Guard: only once per day change
    if (time.day == lastSettlementDay_) return;
    
    // Settlement happens at midnight (hour 0, minute 0)
    if (time.hour == 0 && time.minute == 0) {
        lastSettlementDay_ = time.day;
        
        // 1. Daily Settlement — publish event with yesterday's totals
        Event dailyEv;
        dailyEv.type = EventType::DailySettlement;
        dailyEv.payload = DailySettlementPayload{time.day, dailyIncome_, dailyExpense_, balance_};
        eventBus_.publish(dailyEv);
        
        // Accumulate weekly/quarterly totals BEFORE resetting daily
        weeklyIncome_ += dailyIncome_;
        weeklyExpense_ += dailyExpense_;
        quarterlyIncome_ += dailyIncome_;
        
        // Reset daily counters
        dailyIncome_ = 0;
        dailyExpense_ = 0;
        spdlog::debug("EconomyEngine: daily settlement for day {}, weekly income={}, quarterly income={}", 
                      time.day, weeklyIncome_, quarterlyIncome_);
        
        // 2. Weekly Report (every 7 days)
        if (time.day > 0 && time.day % 7 == 0) {
            Event weeklyEv;
            weeklyEv.type = EventType::WeeklyReport;
            weeklyEv.payload = WeeklyReportPayload{time.day / 7, weeklyIncome_, weeklyExpense_};
            eventBus_.publish(weeklyEv);
            
            weeklyIncome_ = 0;
            weeklyExpense_ = 0;
            spdlog::debug("EconomyEngine: weekly report for week {}", time.day / 7);
        }
        
        // 3. Quarterly Settlement (every 90 days)
        if (time.day > 0 && time.day % 90 == 0) {
            // Tax = balance * taxRate (configurable, default 5%) — spec §5.20
            int64_t tax = static_cast<int64_t>(balance_ * config_.quarterlyTaxRate);
            if (tax > 0) addExpense("tax", tax, time);
            
            Event quarterlyEv;
            quarterlyEv.type = EventType::QuarterlySettlement;
            quarterlyEv.payload = QuarterlySettlementPayload{time.day / 90, tax, balance_};
            eventBus_.publish(quarterlyEv);
            
            // Star rating re-evaluation — spec §5.20 item 3
            Event starReEvalEv;
            starReEvalEv.type = EventType::StarRatingReEvalRequested;
            eventBus_.publish(starReEvalEv);
            
            quarterlyIncome_ = 0;
            spdlog::debug("EconomyEngine: quarterly settlement for quarter {}, tax={} ({}% of balance={})", 
                          time.day / 90, tax, static_cast<int>(config_.quarterlyTaxRate * 100), balance_);
        }
    }
}

int64_t EconomyEngine::getDailyIncome() const {
    return dailyIncome_;
}

int64_t EconomyEngine::getDailyExpense() const {
    return dailyExpense_;
}

std::vector<IncomeRecord> EconomyEngine::getRecentIncome(int count) const {
    if (count <= 0) {
        return {};
    }
    
    size_t actualCount = std::min(static_cast<size_t>(count), incomeHistory_.size());
    return std::vector<IncomeRecord>(incomeHistory_.begin(), incomeHistory_.begin() + actualCount);
}

std::vector<ExpenseRecord> EconomyEngine::getRecentExpenses(int count) const {
    if (count <= 0) {
        return {};
    }
    
    size_t actualCount = std::min(static_cast<size_t>(count), expenseHistory_.size());
    return std::vector<ExpenseRecord>(expenseHistory_.begin(), expenseHistory_.begin() + actualCount);
}

void EconomyEngine::addIncomeRecord(const IncomeRecord& record) {
    // Insert at front (newest first)
    incomeHistory_.insert(incomeHistory_.begin(), record);
    
    // Cap at 100 entries
    if (incomeHistory_.size() > 100) {
        incomeHistory_.resize(100);
    }
}

void EconomyEngine::addExpenseRecord(const ExpenseRecord& record) {
    // Insert at front (newest first)
    expenseHistory_.insert(expenseHistory_.begin(), record);
    
    // Cap at 100 entries
    if (expenseHistory_.size() > 100) {
        expenseHistory_.resize(100);
    }
}

// ── SaveLoad export/import ───────────────────────────────────────────────────

nlohmann::json EconomyEngine::exportState() const {
    using json = nlohmann::json;
    json j;
    j["balance"]      = balance_;
    j["dailyIncome"]  = dailyIncome_;
    j["dailyExpense"] = dailyExpense_;
    j["weeklyIncome"] = weeklyIncome_;
    j["weeklyExpense"] = weeklyExpense_;
    j["quarterlyIncome"] = quarterlyIncome_;
    j["lastRentDay"]  = lastRentDay_;
    j["lastMaintenanceDay"] = lastMaintenanceDay_;
    j["lastSettlementDay"] = lastSettlementDay_;

    json incArr = json::array();
    for (const auto& r : incomeHistory_) {
        incArr.push_back({
            {"tenantEntity", static_cast<uint32_t>(r.tenantEntity)},
            {"type",         static_cast<int>(r.type)},
            {"amount",       r.amount},
            {"day",          r.timestamp.day},
            {"hour",         r.timestamp.hour},
            {"minute",       r.timestamp.minute}
        });
    }
    j["incomeHistory"] = incArr;

    json expArr = json::array();
    for (const auto& r : expenseHistory_) {
        expArr.push_back({
            {"category", r.category},
            {"amount",   r.amount},
            {"day",      r.timestamp.day},
            {"hour",     r.timestamp.hour},
            {"minute",   r.timestamp.minute}
        });
    }
    j["expenseHistory"] = expArr;

    return j;
}

void EconomyEngine::importState(const nlohmann::json& j) {
    balance_            = j.value("balance", int64_t(0));
    dailyIncome_        = j.value("dailyIncome", int64_t(0));
    dailyExpense_       = j.value("dailyExpense", int64_t(0));
    weeklyIncome_       = j.value("weeklyIncome", int64_t(0));
    weeklyExpense_      = j.value("weeklyExpense", int64_t(0));
    quarterlyIncome_    = j.value("quarterlyIncome", int64_t(0));
    lastRentDay_        = j.value("lastRentDay", -1);
    lastMaintenanceDay_ = j.value("lastMaintenanceDay", -1);
    lastSettlementDay_  = j.value("lastSettlementDay", -1);

    incomeHistory_.clear();
    if (j.contains("incomeHistory")) {
        for (const auto& r : j["incomeHistory"]) {
            IncomeRecord rec;
            rec.tenantEntity   = static_cast<EntityId>(r.value("tenantEntity", uint32_t(0)));
            rec.type           = static_cast<TenantType>(r.value("type", 0));
            rec.amount         = r.value("amount", int64_t(0));
            rec.timestamp.day    = r.value("day", 0);
            rec.timestamp.hour   = r.value("hour", 0);
            rec.timestamp.minute = r.value("minute", 0);
            incomeHistory_.push_back(rec);
        }
    }

    expenseHistory_.clear();
    if (j.contains("expenseHistory")) {
        for (const auto& r : j["expenseHistory"]) {
            ExpenseRecord rec;
            rec.category         = r.value("category", std::string(""));
            rec.amount           = r.value("amount", int64_t(0));
            rec.timestamp.day    = r.value("day", 0);
            rec.timestamp.hour   = r.value("hour", 0);
            rec.timestamp.minute = r.value("minute", 0);
            expenseHistory_.push_back(rec);
        }
    }

    spdlog::debug("EconomyEngine::importState: balance={}, income records={}, expense records={}",
                  balance_, incomeHistory_.size(), expenseHistory_.size());
}

} // namespace vse