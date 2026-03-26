# TASK-04-001 Review
Reviewer: GPT-5.4
Commit: 9ec0fc7

## Verdict
Fail

## P1 Issues

### 1. `PlaceTenant` command bypasses `TenantSystem`, so placed tenants are not fully created as domain tenants
- `src/core/Bootstrapper.cpp:367-373` charges money, then calls `grid_->placeTenant(..., INVALID_ENTITY)` directly.
- This skips `TenantSystem::placeTenant()` (`src/domain/TenantSystem.cpp:16-86`), which is the domain entry point that creates the tenant entity, attaches `TenantComponent`, and publishes lifecycle events.
- Result: player-placed tenants can exist on the grid without a valid tenant entity/component. That breaks the economy loop you just activated:
  - `EconomyEngine::collectRent()` ignores them because it only collects from anchor tiles with `tenantEntity != INVALID_ENTITY` (`src/domain/EconomyEngine.cpp:89-106`).
  - `TenantSystem::onDayChanged()` tenant maintenance and future eviction logic also miss them because it iterates `TenantComponent` in the registry (`src/domain/TenantSystem.cpp:143-156`, `159-205`).
- This is both a correctness bug and a layer/spec mismatch: spec §5.18 says `TenantSystem::placeTenant()` owns tenant placement rules, but the commit moved placement logic into `Bootstrapper` (Core Runtime).

### 2. Construction cost is deducted before validating that construction actually succeeds
- `src/core/Bootstrapper.cpp:325-327` deducts floor cost, then calls `grid_->buildFloor(...)` and ignores the result.
- `src/core/Bootstrapper.cpp:367-373` deducts tenant cost, then calls `grid_->placeTenant(...)` and ignores the result.
- If the floor is invalid/already built, or the tenant placement is out of range / on an unbuilt or occupied tile, the player is still charged with no rollback.
- This violates the intended contract in §5.18 (“deducts cost from balance; rejects if ...”) because non-fund rejection paths still consume funds.

### 3. Economy rules were added to `Bootstrapper` (Core Runtime), violating the documented layer boundary
- Spec/CLAUDE define Core Runtime as generic plumbing only, with game rules confined to Layer 1.
- In this commit, funds validation, build pricing, insufficient-funds action routing, and tenant build-cost lookup live in `src/core/Bootstrapper.cpp:321-386`.
- Those are game-specific economy/construction rules, not generic runtime orchestration.
- The design spec for §5.18 explicitly assigns these checks to domain-facing operations (`GridSystem::buildFloor()` / `TenantSystem::placeTenant()`), not the composition root.

## P2 Issues

### 1. New tests do not cover the real command path and miss the main regressions above
- `tests/test_EconomyLoop.cpp:88-166` does not execute `Bootstrapper::processCommands()` or any real command handling; it manually calls `economy->addExpense(...)` and checks a precondition boolean instead.
- `tests/test_EconomyLoop.cpp:207-211` claims `InsufficientFunds` coverage but only does `REQUIRE(true)`.
- Because of that, the suite would still pass even though actual `PlaceTenant` command handling creates non-rentable/non-maintained tenants and charges on failed placement/build.

## Summary
The commit adds the missing economy-loop pieces, but the command integration is incorrect: `PlaceTenant` bypasses the domain tenant lifecycle, and both build actions charge before confirming success. The current tests are too shallow to catch either problem.