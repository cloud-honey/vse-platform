# TASK-03-003 Review — Gemini 3.1 Pro
**Reviewer:** google/gemini-3.1-pro-preview
**Verdict:** Fail

## P0 (Critical — must fix before next task)
1. **Eviction Reset Logic Missing:** `TenantSystem::update` unconditionally decrements `evictionCountdown` if it is > 0 and evicts the tenant. It fails to check if satisfaction has recovered above `evictionThreshold_` to cancel/reset the countdown.
2. **EconomyEngine API Bypass:** `TenantSystem::onDayChanged` loops through tenants and calls `addIncome`/`addExpense` directly. Spec §5.7 explicitly delegates this responsibility to `IEconomyEngine::collectRent` and `IEconomyEngine::payMaintenance`. Doing it in TenantSystem violates the design and risks double-processing.
3. **Missing Event Payloads:** Events like `TenantPlaced` and `RentCollected` are published without their required `ev.payload` data. Spec §5.2 and `Types.h` require structs like `TenantPlacedPayload` and `RentCollectedPayload`. Listeners will fail when trying to cast empty payloads.

## P1 (Important — fix before Sprint end)
1. **Hardcoded ContentRegistry Values:** `placeTenant` hardcodes rent, maintenance, and capacity values instead of reading from `ContentRegistry::getTenantDef()`.
2. **Hardcoded Balance Values:** `evictionThreshold_` is hardcoded to `20.0f` instead of reading from `balance.json`.
3. **AgentComponent Spec Mismatch:** `computeAverageSatisfactionForTenant` uses `agent.workplaceTenant` (EntityId), but Spec §5.5 only defines `TenantType workplace` and `EntityId homeTenant`. If an EntityId for workplace was added, it must be synced to the design spec.

## P2 (Nice-to-have)
1. **activeTenantCount excludes evicting tenants:** Tenants with `evictionCountdown > 0` are excluded from the active count, which may cause UI state mismatches while they are still physically present in the building.
2. **Misunderstood EventBus API:** Developer notes state EventBus lacks deferred publishing. However, per Spec §5.2, `bus_.publish()` is *always* deferred. The code is structurally correct, but the TODO comment should be removed.

## Summary
The implementation successfully structures the tenant lifecycle ECS components but fails critically on eviction reset logic, bypasses the EconomyEngine's explicit collection APIs, and omits mandatory event payloads. These architectural and logical violations must be fixed.