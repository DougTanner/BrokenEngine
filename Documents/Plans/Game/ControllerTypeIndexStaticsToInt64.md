<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T13:14:18.504Z","dependsOn":[]} -->
# Store the static controller type indices as int64_t

## Context

`Documents/C++StyleGuide.txt` rule 13 (lines 101-106) defaults our own variables and members to `int64_t`. Seven `static inline` controller type indices are still `uint8_t`, although they are startup-registered handles that are never packed into an array, never serialized, and never CRC'd. Every reader already widens them to `int64_t`, so each registration round-trips through an `int64_t` local and a `static_cast<uint8_t>` back:

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:68-69` `suiImpactPuffControllerTypeIndex`, `suiImpactPointLightControllerTypeIndex`; round trip at `Players.cpp:128-148` and `:157-174`; read at `PlayersCombat.cpp:245-246` as the `int64_t iControllerTypeIndex` parameter of `PuffsPostRender::AddControlled` (`Engine/Source/Frame/Collections/Puffs/Puffs.h:98`) and `PointLightsPostRender::AddControlled` (`Engine/Source/Frame/Collections/PointLights/PointLights.h:91`).
- `Engine/Source/Frame/Collections/Explosions/Explosions.h:177-180,182` `suiPrimaryLightControllerTypeIndex`, `suiSecondaryLightControllerTypeIndex`, `suiPrimaryPuffControllerTypeIndex`, `suiSecondaryPuffControllerTypeIndex`, `suiWindRadialControllerTypeIndex`, each initialized `static_cast<uint8_t>(kiInvalidControllerType)`; round trip at `Explosions.cpp:91-110`, `:112-131`, `:139-156`, `:158-175`, `:184-192`; read into the `int64_t` `ExplosionType` fields (`Explosions.h:89-94`) at `Players.cpp:91-96`, `Missiles.cpp:220-225`, `Spaceships.cpp:112-117`.

Neighbouring statics already use the target shape: `Players.h:65-67,70` (`int64_t siAreaLightTypeIndex = 0xFF` and siblings) and `Explosions.h:181` (`int64_t siExplosionTrailTypeIndex`). `ControllerTypeRegistry::RegisterControllerType` takes `int64_t&` and asserts the sentinel (`Engine/Source/Frame/Collections/CollectionController.h:117-119`), whose value is `kiInvalidTypeIndex = 0xFF` (`:12-13`).

Origin: `Documents/Investigations/ChangeWorkflow/OwnIntegerTypeSweepDeferredFixes.md` (Players F7, F8), deleted when the Plans are created. That entry assumed a Collection member; current source shows plain statics. The five Explosions statics share the root cause and the round trip, though the investigation did not list them.

## Design

The author recommends:

- Declare all seven as `static inline int64_t` with the `si` prefix (`siImpactPuffControllerTypeIndex`, `siImpactPointLightControllerTypeIndex`, `siPrimaryLightControllerTypeIndex`, `siSecondaryLightControllerTypeIndex`, `siPrimaryPuffControllerTypeIndex`, `siSecondaryPuffControllerTypeIndex`, `siWindRadialControllerTypeIndex`), initialized to `kiInvalidControllerType` — the same value 255 the `uint8_t` held, so registration asserts and every `!= kiInvalidControllerType` comparison see the same value.
- Pass each static directly to `RegisterControllerType`, deleting the `int64_t` local and the `static_cast<uint8_t>` write-back at each registration.
- Rename the readers at `PlayersCombat.cpp:245-246`, `Players.cpp:91-96`, `Missiles.cpp:220-225`, `Spaceships.cpp:112-117`.

Behavior is identical: every stored value is either 255 or a registration index below 255, both representable in either type. No layout, serialization, CRC, wire, or replay byte changes.

Risk tier: Tier 2 (`.agents/references/risk-tiers.md`) — public statics in the engine header `Explosions.h:177-180,182`, read by game code, change type and name, so Tier 1's no-public-signature condition fails; every reader already consumes `int64_t` and no runtime value changes.

## Critical files

- `Engine/Source/Frame/Collections/Explosions/Explosions.h`, `Explosions.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h`, `Players.cpp`, `PlayersCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp` (reader renames only)

## In scope

- The seven static declarations above, their registration sites in `Explosions.cpp` and `Players.cpp`, and every reader named in `## Context`.

## Out of scope

- Collection columns, including `puiTypeIndices` and `puiControllerTypeIndices` in every collection: their width is a serialized Collection layout.
- `kiInvalidTypeIndex`, `kiInvalidControllerType`, and both registries' signatures and asserts.
- Every other static or type index already declared `int64_t`.

## Acceptance criteria

- No `uint8_t` controller type index static remains in `Explosions.h` or `Players.h`, and no `static_cast<uint8_t>` remains at those registrations.
- Client and server builds succeed.

## Notes

- Verification: `/compile` client and server; no harness run, since no runtime value changes.

## Coordination

- `Documents/Plans/Game/ExplosionParticleCountsToInt64.md` edits the same `ExplosionType` initializers (`Players.cpp:91-98`, `Missiles.cpp:220-227`, `Spaceships.cpp:112-119`) and `Explosions.h`. Whichever lands second rebases onto the other's edits.
