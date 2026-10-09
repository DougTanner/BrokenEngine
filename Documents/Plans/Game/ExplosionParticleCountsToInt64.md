<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T13:14:09.115Z","dependsOn":[]} -->
# Store explosion particle counts as int64_t

## Context

`Documents/C++StyleGuide.txt` rule 13 (lines 101-106) defaults our own members to `int64_t`. Two explosion particle-count fields are `uint32_t` although neither is packed, serialized, or CRC'd:

- `ExplosionType::uiBaseParticleCount` (`Engine/Source/Frame/Collections/Explosions/Explosions.h:96`), set at registration as `static_cast<uint32_t>` of an `int64_t` constant: `Players.cpp:98`, `Missiles.cpp:227`, `Spaceships.cpp:119`.
- `ExplosionsPostRender::SpawnInfo::uiParticleCount` (`Explosions.h:263`), set as `static_cast<uint32_t>` of a float product: `PlayersCombat.cpp:475`, `Missiles.cpp:272`, `Spaceships.cpp:239`. Its sibling `SpawnInfo::iTrailCount` (`Explosions.h:261`) is already `int64_t`, filled by `static_cast<int64_t>` of a float product at `Missiles.cpp:270`.

`ExplosionsSpawn.cpp:168` sums the two as `uint32_t` before assigning `int64_t iTotalParticles`, which bounds the particle loop at `:194`; each iteration draws from the shared `rFrame.postRender.randomEngine`, so the count is deterministic, CRC-relevant input.

Origin: `Documents/Investigations/ChangeWorkflow/OwnIntegerTypeSweepDeferredFixes.md` (Explosions F10, F16), deleted when the Plans are created. The investigation kept the fields because widening removes the `uint32_t` wrap of that sum. The wrap is unreachable: the constants are 15-16 (`Missiles.cpp:45`, `Players.cpp:36`, `Spaceships.cpp:56`) and the float products are at most 2 x 15 (`Missiles.cpp:52`, `:254-257`, `:272`; size multipliers at most 1), 16 (`PlayersCombat.cpp:44`, `:457`, `:475`), and 8 (`Spaceships.cpp:37`, `:427`, `:239`). Every product is non-negative: `fPercent` divides a destroyed time that `PlayersCombat.cpp:450` requires to be positive and that `Spaceships.cpp:286-288` floors at zero while `kExploding` is set (`SpaceshipsCombat.cpp:58-59`), and missiles pass `1.0f` (`Missiles.cpp:512`). For non-negative values below 2^32, `static_cast<int64_t>` and `static_cast<uint32_t>` of a float truncate identically, so the loop length and random draw count are unchanged for every reachable input.

## Design

The author recommends:

- Declare `int64_t iBaseParticleCount = 0;` and `int64_t iParticleCount = 0;` in place of the two fields.
- At the three registration sites, assign the `int64_t` constant directly; at the three spawn sites, use `static_cast<int64_t>(...)` of the same float expression, matching `iTrailCount` at `Missiles.cpp:270`.
- `ExplosionsSpawn.cpp:168` becomes `rSpawnInformation.iParticleCount + rType.iBaseParticleCount`, with no other change.

No collection column, serialized byte, wire field, or CRC input changes; the shared CRC stays identical tick for tick.

Risk tier: Tier 2 (`.agents/references/risk-tiers.md`) — public struct fields shared by the engine and game collections change type, feeding a deterministic loop whose computed result does not change. A reviewer who treats the RNG-driving count as a determinism surface may escalate to Tier 3.

## Critical files

- `Engine/Source/Frame/Collections/Explosions/Explosions.h`, `ExplosionsSpawn.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp`, `PlayersCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp`

## In scope

- `ExplosionType::uiBaseParticleCount`, `ExplosionsPostRender::SpawnInfo::uiParticleCount`, the sum at `ExplosionsSpawn.cpp:168`, and the six assignment sites named in `## Context`.

## Out of scope

- `ExplosionType::uiParticleColor` (`Explosions.h:98`): a packed color.
- Every `ExplosionsInterpolate` or `ExplosionsPostRender` column, including `puiTypeIndices` and `piTrailCounts`.
- The float particle-count constants and the percent expressions.

## Acceptance criteria

- No `uint32_t` particle count remains in `Explosions.h`, and no `static_cast<uint32_t>` remains at the six assignment sites.
- Client and server builds succeed.
- A replay recorded before the change replays with matching per-tick CRCs, through `/agent-harness`'s replay determinism check, across a run that destroys at least one player, one spaceship, and one missile.

## Notes

- Verification: `/compile` client and server; `/agent-harness` replay determinism check for the CRC criterion.

## Coordination

- `Documents/Plans/Game/ControllerTypeIndexStaticsToInt64.md` edits the same `ExplosionType` initializers (`Players.cpp:91-98`, `Missiles.cpp:220-227`, `Spaceships.cpp:112-119`) and `Explosions.h`. Whichever lands second rebases onto the other's edits.
