# Cross-Validation Review — TASK-02-004

> Reviewer: Gemini 3 Flash
> Verdict: Conditional Pass

## Issues Found

### P1 — Fix Soon

**Lack of Hysteresis (Event Flickering)**
- Problem: Satisfaction hovering around a threshold (e.g. 69.9% → 70.1%) will spam `StarRatingChanged` events every tick — UI flickering, repeated SFX triggers.
- Recommendation: Add a buffer zone or cooldown period before rating change is finalized, or require value to stay above/below threshold for N ticks.

**initRegistry Idempotency**
- Problem: If called twice (scene reload, re-init), creates duplicate `StarRatingComponent` entities → undefined behavior in `update()` view fetch.
- Recommendation: Use `reg.ctx().emplace<StarRatingComponent>()` (EnTT context variable, designed for singletons), or guard with `reg.storage<StarRatingComponent>().empty()`.

### P2 — Nice to Have

**Layer Boundary: StarRatingChangedPayload in Layer 1**
- Problem: Renderer (Layer 3) must include a Domain header to subscribe to this event — tight coupling bypassing Core API.
- Recommendation: Move to `Types.h` or dedicated `Events.h` in Core layer (tracked in Open Items).

**const_cast Technical Debt**
- Problem: `const_cast<entt::registry&>` masks that `IAgentSystem::getAverageSatisfaction()` is not properly const.
- Recommendation: Refactor `IAgentSystem` to accept `const entt::registry&` (tracked in Open Items).
