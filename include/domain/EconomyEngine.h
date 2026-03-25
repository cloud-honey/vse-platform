/**
 * @file EconomyEngine.h
 * @layer Layer 1 — Domain Module
 * @task TASK-02-003
 * @author DeepSeek V3
 * @reviewed pending
 *
 * @brief EconomyEngine — concrete implementation of IEconomyEngine.
 *        Reads rent/maintenance rates from balance config passed at construction.
 *        Collects rent on DayChanged, pays maintenance on DayChanged.
 *
 * @note All monetary values use int64_t Cents to avoid float precision issues
 *       and integer overflow at large balances.
 *       Max safe balance: INT64_MAX Cents (~9.2 * 10^18).
 *       Overflow guard: if addIncome/addExpense would overflow, log warning and clamp.
 *
 * @see IEconomyEngine.h
 * @see VSE_Design_Spec.md §5.7
 */

#pragma once
#include "core/IEconomyEngine.h"
#include "core/IGridSystem.h"
#include <vector>
#include <cstdint>
#include <string>

namespace vse {

struct EconomyConfig {
    int64_t startingBalance;          // Cents — from balance.json "economy.starting_balance"
    int64_t officeRentPerTilePerDay;  // Cents — from balance.json "economy.office_rent_per_tile_per_day"
    int64_t residentialRentPerTilePerDay; // Cents
    int64_t commercialRentPerTilePerDay;  // Cents
    int64_t elevatorMaintenancePerDay;    // Cents per elevator shaft per day
};

class EconomyEngine : public IEconomyEngine {
public:
    explicit EconomyEngine(const EconomyConfig& config);
    
    // IEconomyEngine implementation
    int64_t getBalance() const override;
    void addIncome(EntityId tenant, TenantType type, int64_t amount, const GameTime& time) override;
    void addExpense(const std::string& category, int64_t amount, const GameTime& time) override;
    
    void collectRent(const IGridSystem& grid, const GameTime& time) override;
    void payMaintenance(const IGridSystem& grid, const GameTime& time) override;
    
    void update(const GameTime& time) override;
    
    int64_t getDailyIncome()  const override;
    int64_t getDailyExpense() const override;
    
    std::vector<IncomeRecord>  getRecentIncome(int count)   const override;
    std::vector<ExpenseRecord> getRecentExpenses(int count) const override;

private:
    int64_t balance_;
    EconomyConfig config_;
    std::vector<IncomeRecord>  incomeHistory_;   // capped at 100 entries
    std::vector<ExpenseRecord> expenseHistory_;  // capped at 100 entries
    int64_t dailyIncome_  = 0;
    int64_t dailyExpense_ = 0;
    int     lastRentDay_ = -1;        // guard: collect rent only once per game day
    int     lastMaintenanceDay_ = -1; // guard: pay maintenance only once per game day
    
    void addIncomeRecord(const IncomeRecord& record);
    void addExpenseRecord(const ExpenseRecord& record);
};

} // namespace vse