# VSE PLATFORM — Cross-Validation Prompt
## For GPT-4o / Gemini Ultra / DeepSeek-R1
### Paste this document directly into any AI model

---

## How to Use This Prompt

This document is submitted to multiple AI models simultaneously for cross-validation.
The goal is to compare responses and identify consensus vs. disagreement.

- **Items all models agree on** → high confidence, must reflect in final design
- **Items models disagree on** → needs further review
- **Items only one model flags as a risk** → treat as a potential blind spot

This same prompt has been submitted to: Claude Opus 4, GPT-4o, Gemini Ultra, DeepSeek-R1

---

## Project Summary

We are building a **general-purpose management simulation engine (VSE: Vertical Sim Engine)**,
starting with a building management game (Tower Tycoon) as the first title,
then expanding into a Building Automation System (BAS) digital twin and educational platform.

**Tech stack:** SDL2 + C++17, CMake, targeting iOS / Android / Steam simultaneously
**Development method:** Vibe coding with multi-agent AI workflow
**Developer background:** Solo developer transitioning into BAS engineering (BACnet, Modbus, DDC), South Korea

### The 4-Phase Roadmap

| Phase | Goal | Timeline |
|---|---|---|
| 1 | Tower Tycoon MVP → Steam release (single-player building sim) | 0–6 months |
| 2 | Real building economics + BAS digital twin integration | 6–12 months |
| 3 | Online multiplayer (20–50 players per city) + BAS training platform | 12–24 months |
| 4 | VSE open-source, Steam Workshop, real city data, B2B licensing | 24 months+ |

### Core Game Systems (Tower Tycoon)
- Tile grid placement (375 segments × N floors)
- Elevator simulation: LOOK algorithm, max 24 shafts, max 8 cars per shaft, max 1 transfer per NPC path
- NPC AI: time-of-day behavior patterns, stress system, pathfinding with 4-floor stair limit
- Economy: rent collection, construction costs, quarterly settlement
- Disasters: fire, cockroaches, bomb threats
- Star rating system: ★1 through TOWER

---

## Proposed Architecture (4 Layers)

### Layer 0 · VSE Core (never modified per game title)
`GridSystem`, `AgentSystem`, `TransportSystem`, `EventBus`, `EconomyEngine`,
`SimClock`, `LocaleManager`, `ContentRegistry`, `ConfigManager`,
`AuditLogger`, `INetworkAdapter`, `VariableEncryption`

### Layer 1 · Domain Module (per-game logic)
Implements VSE Core interfaces.
First implementation: `RealEstateDomain` (rent, vacancy, district coefficients, problem tenants)
Second game: `ApartmentDomain` — must be buildable without modifying Layer 0 (this is the design pass/fail test)

### Layer 2 · Content Pack (data + assets only)
JSON + sprites. No code changes to add new building types, events, or DLC.
`building_types.json`, `events.json`, `balance.json`, `locale/en.json`

### Layer 3 · Renderer + Platform (SDL2)
`IRenderCommand`-based. Dirty Rect rendering.
`SDL2Renderer` → swappable to `VulkanRenderer` without touching game logic.
Separate input handlers: `MouseKeyboardInput` / `TouchInput` / `ControllerInput`

---

## 5 Key Design Decisions — Please Evaluate Each

### Decision 1: All numeric values in external config files
No hardcoded economy values, NPC parameters, or event probabilities in source code.
All values read from `balance.json` at runtime — editable via Admin Console without recompiling.

**Question:** Is this the right approach? What are the risks? What would you do differently?

### Decision 2: EventBus — all game events are serializable
Every game event (tenant evicted, fire started, rent collected) flows through a central EventBus and is serializable via FlatBuffers or MessagePack.
This is the prerequisite for online multiplayer: single-player uses LocalAdapter, online swaps to RemoteAdapter.

**Question:** Is EventBus the right pattern for this scale? Any concerns with performance or complexity?

### Decision 3: INetworkAdapter — LocalAdapter / RemoteAdapter swap
```cpp
class INetworkAdapter {
    virtual void send(const GameEvent& event) = 0;
    virtual GameEvent receive() = 0;
};
class LocalAdapter : public INetworkAdapter { /* processes locally */ };
class RemoteAdapter : public INetworkAdapter { /* sends to server */ };
```
Game logic always calls `INetworkAdapter::send()` — never knows if it's single-player or online.

**Question:** Will this pattern hold when transitioning from single-player to an authoritative server model?

### Decision 4: Tenant as a first-class object
```cpp
struct Tenant {
    UUID            tenant_id;
    TenantType      type;
    Contract        contract;        // [Phase 2] lease term, rent, deposit
    PersonalityProfile personality;  // [Phase 2] nuisance index, complaint tendency
    float           satisfaction;
    float           stress;
    LegalStatus     legal_status;    // [Phase 2] NORMAL / OVERDUE / DISPUTE / EVICTION
    std::vector<ContractHistory> history; // [Phase 3]
};
```
Fields declared now, logic implemented in later phases.

**Question:** Is pre-declaring Phase 2–3 fields in Phase 1 structs a good practice, or does it create technical debt?

### Decision 5: LOOK algorithm for elevator scheduling
- LOOK algorithm (does not travel in direction with no requests)
- 14 time-slot daily schedule with per-slot config: `response_distances`, `express_modes`, `departure_delays`
- NPC pathfinding: max 1 elevator transfer, max 4 floors by stairs
- Sky lobby: express elevators stop only at multiples of 15F

**Question:** Is LOOK the right algorithm? What edge cases will break it with 8 cars per shaft?

---

## Validation Questions

### Q1. Architecture Completeness
Does the 4-layer VSE architecture cover everything needed?
What module is missing from Layer 0?
Which layer would need changes to support a Cruise Ship domain vs. the Building domain?

### Q2. Technology Stack Risk
SDL2 + C++17 + CMake for iOS / Android / Steam simultaneously:
- What are the top 3 risks of this stack for a solo vibe-coding developer?
- Is this realistic given an AI-assisted development workflow?
- What would you recommend instead, and why?

### Q3. Biggest Project Risks
List the top 3 risks for this project across these categories:
- Technical (implementation difficulty, performance, architecture complexity)
- Business (market, competition, monetization)
- Process (vibe coding limits, context management, solo development)

For each risk, suggest a mitigation strategy.

### Q4. What's Missing
What has this design explicitly not addressed that must be decided before Phase 1 begins?
Be specific — not general advice, but concrete missing decisions.

### Q5. BAS Integration Feasibility
The core game systems map directly to real BAS concepts:

| Game Element | Real BAS System | Protocol |
|---|---|---|
| Elevator AI | Elevator group control | BACnet Elevator Object |
| NPC stress / satisfaction | Occupant comfort (IAQ) | BACnet Analog Input |
| Fire disaster | Fire detection + sprinklers | BACnet Fire Object |
| Time-of-day NPC patterns | Occupancy Schedule | BACnet Schedule Object |

- Is it realistic to integrate `bacnet-stack` (open-source, MIT) into a C++ SDL2 game?
- Can a game UI realistically serve as a building digital twin interface?
- Is there a market for a software-based BAS training simulator? (Currently only hardware simulators exist)

### Q6. VSE as a Platform
The long-term vision: Tower Tycoon ships, then VSE Core is extracted as an open-source library.
Other games built on VSE: Apartment Manager, Cruise Director, Starship Commander.

- Is the proposed abstraction (IEconomyRule, IEventRule, ILocationRule) sufficient to support this?
- What is the biggest architectural mistake that would prevent this vision from working?
- Has anyone shipped something similar? What can we learn from it?

### Q7. Your Disagreement
This prompt is being evaluated by Claude Opus 4, GPT-4o, Gemini Ultra, and DeepSeek-R1.

**What part of this design do you think you will evaluate most differently from the other models, and why?**

---

## Reference: Open-Source Projects

| Project | Language | License | Relevance |
|---|---|---|---|
| OpenSkyscraper | C++ | MIT | Direct SimTower reverse engineering, TDT format docs |
| OpenTTD | C++ | GPL v2 | Transport tycoon — architecture reference only |
| OpenRCT2 | C++ | GPL v3 | NPC satisfaction system — architecture reference only |
| Space Station 14 | C# | MIT | Spaceship management sim — domain reference |
| bacnet-stack | C | MIT | BACnet/IP open-source library, direct integration target |
| Citybound | Rust | AGPL v3 | Actor-model city sim — EventBus architecture reference |

**Note:** GPL projects cannot be used as source code in a commercial product.
Ideas and architecture patterns can be referenced; code cannot be copied.

---

## AI Team Division of Labor (Context)

| AI Model | Role |
|---|---|
| Claude Opus 4 | Overall architecture design, complex algorithm design |
| DeepSeek-R1 | Pure algorithm design (elevator scheduling math, pathfinding) |
| Claude Code (Sonnet) | SDL2 integration code, actual builds |
| GPT-4o / Gemini | Sprite design, UI layout |
| Developer (human) | Integration judgment, bug root cause, game feel decisions |

---

*Cross-validation prompt for VSE Platform architecture.*
*Compare responses across models. Consensus = high confidence. Divergence = investigate further.*
