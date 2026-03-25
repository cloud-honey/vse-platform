/**
 * @file IEconomyEngine.h
 * @layer Core API (include/core/) — interface only, no concrete implementation
 * @task TASK-02-003
 * @author DeepSeek V3
 * @reviewed pending
 *
 * @brief Economy engine interface.
 *        Manages building income (rent) and expenses (maintenance).
 *        Currency unit: int64_t Cents (1 Cent = smallest unit).
 *
 * @note balance.json is the single source for all economy values.
 *       IEconomyEngine does NOT read ConfigManager directly.
 *       Caller (Bootstrapper) passes balance values at construction time.
 *
 * @see VSE_Design_Spec.md §5.7
 * @see CLAUDE.md §Layer 0 Module List
 */

#pragma once
#include "core/Types.h"
#include "core/Error.h"
#include "core/IGridSystem.h"
#include <vector>
#include <string>

namespace vse {

struct IncomeRecord {
    EntityId    tenantEntity;
    TenantType  type;
    int64_t     amount;       // Cents
    GameTime    timestamp;
};

struct ExpenseRecord {
    std::string category;     // "rent", "maintenance", "elevator"
    int64_t     amount;       // Cents
    GameTime    timestamp;
};

class IEconomyEngine {
public:
    virtual ~IEconomyEngine() = default;

    virtual int64_t getBalance() const = 0;
    virtual void addIncome(EntityId tenant, TenantType type, int64_t amount, const GameTime& time) = 0;
    virtual void addExpense(const std::string& category, int64_t amount, const GameTime& time) = 0;

    // Called once per game day (on DayChanged event)
    virtual void collectRent(const IGridSystem& grid, const GameTime& time) = 0;
    virtual void payMaintenance(const IGridSystem& grid, const GameTime& time) = 0;

    // Called every tick by Bootstrapper
    virtual void update(const GameTime& time) = 0;

    virtual int64_t getDailyIncome()  const = 0;
    virtual int64_t getDailyExpense() const = 0;

    // Returns up to `count` most recent records (newest first)
    virtual std::vector<IncomeRecord>  getRecentIncome(int count)   const = 0;
    virtual std::vector<ExpenseRecord> getRecentExpenses(int count) const = 0;
};

} // namespace vse