/**
 * @file test_Settlement.cpp
 * @task TASK-04-003
 * @author DeepSeek V3 (structure) + 붐 (completion)
 * @brief Periodic settlement tests — daily/weekly/quarterly events, tax deduction.
 */

#include "domain/EconomyEngine.h"
#include "core/EventBus.h"
#include <catch2/catch_test_macros.hpp>

using namespace vse;

static EconomyConfig makeSettlementConfig() {
    return EconomyConfig{
        1000000, 500, 300, 800, 1000, 10000
    };
}

// Test 1: Daily settlement fires at day change
TEST_CASE("Settlement - daily settlement event fires", "[Settlement]") {
    EventBus bus;
    EconomyEngine economy(makeSettlementConfig(), bus);

    bool dailyFired = false;
    int capturedDay = -1;
    bus.subscribe(EventType::DailySettlement, [&](const Event& e) {
        dailyFired = true;
        if (auto* p = std::any_cast<DailySettlementPayload>(&e.payload)) {
            capturedDay = p->day;
        }
    });

    // Add some income/expense on day 0
    economy.addIncome(INVALID_ENTITY, TenantType::Office, 5000, GameTime{0, 9, 0});
    economy.addExpense("test", 2000, GameTime{0, 9, 0});

    // Day 1 midnight → settlement
    economy.update(GameTime{1, 0, 0});
    bus.flush();

    REQUIRE(dailyFired);
    REQUIRE(capturedDay == 1);
}

// Test 2: Daily counters reset after settlement
TEST_CASE("Settlement - daily counters reset after settlement", "[Settlement]") {
    EventBus bus;
    EconomyEngine economy(makeSettlementConfig(), bus);

    economy.addIncome(INVALID_ENTITY, TenantType::Office, 5000, GameTime{0, 9, 0});
    economy.addExpense("test", 2000, GameTime{0, 9, 0});

    REQUIRE(economy.getDailyIncome() == 5000);
    REQUIRE(economy.getDailyExpense() == 2000);

    // Day 1 midnight
    economy.update(GameTime{1, 0, 0});

    REQUIRE(economy.getDailyIncome() == 0);
    REQUIRE(economy.getDailyExpense() == 0);
}

// Test 3: Weekly report fires every 7 days
TEST_CASE("Settlement - weekly report fires every 7 days", "[Settlement]") {
    EventBus bus;
    EconomyEngine economy(makeSettlementConfig(), bus);

    int weeklyCount = 0;
    int64_t capturedWeeklyIncome = 0;
    bus.subscribe(EventType::WeeklyReport, [&](const Event& e) {
        weeklyCount++;
        if (auto* p = std::any_cast<WeeklyReportPayload>(&e.payload)) {
            capturedWeeklyIncome = p->weeklyIncome;
        }
    });

    // Simulate 7 days with income each day
    for (int day = 0; day < 7; ++day) {
        economy.addIncome(INVALID_ENTITY, TenantType::Office, 1000, GameTime{day, 9, 0});
        if (day > 0) {
            economy.update(GameTime{day, 0, 0});
            bus.flush();
        }
    }
    // Day 7 → weekly report
    economy.update(GameTime{7, 0, 0});
    bus.flush();

    REQUIRE(weeklyCount == 1);
    REQUIRE(capturedWeeklyIncome == 7000); // 1000 * 7 days
}

// Test 4: Quarterly settlement deducts 10% tax
TEST_CASE("Settlement - quarterly settlement deducts 10% tax", "[Settlement]") {
    EventBus bus;
    EconomyEngine economy(makeSettlementConfig(), bus);

    bool quarterlyFired = false;
    int64_t capturedTax = 0;
    bus.subscribe(EventType::QuarterlySettlement, [&](const Event& e) {
        quarterlyFired = true;
        if (auto* p = std::any_cast<QuarterlySettlementPayload>(&e.payload)) {
            capturedTax = p->taxAmount;
        }
    });

    int64_t balanceBefore = economy.getBalance();

    // Simulate 90 days with income
    for (int day = 0; day < 90; ++day) {
        economy.addIncome(INVALID_ENTITY, TenantType::Office, 1000, GameTime{day, 9, 0});
        if (day > 0) {
            economy.update(GameTime{day, 0, 0});
        }
    }
    // Day 90 → quarterly settlement
    economy.update(GameTime{90, 0, 0});
    bus.flush();

    REQUIRE(quarterlyFired);
    REQUIRE(capturedTax == 9000); // 90 days * 1000 = 90000, 10% = 9000
    // Balance: started 1000000 + 90000 income - 9000 tax = 1081000
    REQUIRE(economy.getBalance() == balanceBefore + 90000 - 9000);
}

// Test 5: Settlement only fires once per day
TEST_CASE("Settlement - settlement fires only once per day", "[Settlement]") {
    EventBus bus;
    EconomyEngine economy(makeSettlementConfig(), bus);

    int dailyCount = 0;
    bus.subscribe(EventType::DailySettlement, [&](const Event&) {
        dailyCount++;
    });

    // Call update multiple times on same day
    economy.update(GameTime{1, 0, 0});
    economy.update(GameTime{1, 0, 0});
    economy.update(GameTime{1, 0, 0});
    bus.flush();

    REQUIRE(dailyCount == 1);
}

// Test 6: Quarterly income resets after settlement
TEST_CASE("Settlement - quarterly income resets", "[Settlement]") {
    EventBus bus;
    EconomyEngine economy(makeSettlementConfig(), bus);

    int quarterlyCount = 0;
    bus.subscribe(EventType::QuarterlySettlement, [&](const Event&) {
        quarterlyCount++;
    });

    // First 90 days
    for (int day = 1; day <= 90; ++day) {
        economy.update(GameTime{day, 0, 0});
    }
    bus.flush();
    REQUIRE(quarterlyCount == 1);

    // Next 90 days — should fire again at day 180
    for (int day = 91; day <= 180; ++day) {
        economy.update(GameTime{day, 0, 0});
    }
    bus.flush();
    REQUIRE(quarterlyCount == 2);
}
