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

EconomyEngine::EconomyEngine(const EconomyConfig& config)
    : balance_(config.startingBalance)
    , config_(config) {
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
    // Reset daily counters at start of new day
    if (time.hour == 0 && time.minute == 0) {
        dailyIncome_ = 0;
        dailyExpense_ = 0;
        spdlog::debug("EconomyEngine: reset daily counters for new day");
    }
    
    // Currently no per-tick logic needed
    // Placeholder for future StarRating hooks
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

} // namespace vse