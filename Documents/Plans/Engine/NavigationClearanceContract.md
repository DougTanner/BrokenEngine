<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T21:12:39.058Z","dependsOn":[]} -->
# Replace Player-derived navigation clearance inputs

## Context

The approved D15 boundary requires Engine-owned source and interfaces to avoid
Player-specific headers, Player symbols or collections, Player constants, and
Player semantics. Generic game-provided Frame, Game, session, message, HUD,
and profile headers may retain Player internals when Engine treats them
opaquely.

The navigation clearance leak is evidenced by
`Engine/Source/Frame/NavBuild.cpp:5`, which includes
`Frame/Collections/Players/Players.h`, and `NavBuild.cpp:371`, where
`kfClearanceMeters` is formed from `game::kfPlayerRadius +
game::kfPushMargin`. The same game constants are declared at
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:22-26`.
The startup threshold path is evidenced by
`Engine/Source/Main.cpp:4,253,308`, which includes the same header and passes
that Player-derived clearance to `IslandTerrain::WaitForElevationMaps`, whose
threshold seam is declared at `Engine/Source/Frame/IslandTerrain.h:148` and
whose current implementation accepts but marks `fNavThreshold` unused at
`Engine/Source/Frame/IslandTerrain.cpp:152-155`. The value feeds deterministic
NavBuild contour inflation and remains an observable startup input even though
the current terrain loader does not consume it; both its value and current call
behavior must be preserved.

## Design

Before editing, choose one bounded generic clearance/threshold contract:

1. Pass a typed navigation-clearance/threshold value through the existing
   NavBuild and terrain initialization seams, with the game supplying the
   current values.
2. Define an Engine-owned constexpr/default tuning contract with a game
   override for the selected fields, retaining the current defaults and
   avoiding runtime indirection in deterministic build paths.

The selected contract must preserve the current `kfPlayerRadius +
kfPushMargin` arithmetic, threshold meaning, contour inflation, and client /
server initialization order. Do not move unrelated Player gameplay constants
or add speculative configuration.

## Critical files

- `Engine/Source/Frame/NavBuild.cpp:1-5,344-376,406-411` — the Player
  include, clearance construction, and contour inflation.
- `Engine/Source/Frame/NavBuild.h:64-65` — only if the selected contract
  changes the NavBuild entry points.
- `Engine/Source/Main.cpp:1-8,247-254,306-309` — client/server terrain
  startup threshold callers.
- `Engine/Source/Frame/IslandTerrain.h:148` and
  `Engine/Source/Frame/IslandTerrain.cpp:152-155` — the existing threshold
  boundary.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:22-26`
  — current game-value evidence; Player gameplay ownership remains outside
  this Plan.

## In scope

- Replace the Player-derived clearance and startup threshold inputs used by
  Engine NavBuild/Main with the selected generic contract.
- Remove the direct Player-specific include and `kfPlayerRadius`/
  `kfPushMargin` names from the covered Engine paths.
- Preserve all current default values, arithmetic/precision, threshold
  semantics, contour inflation/tolerances, client/server startup ordering,
  allocation behavior, and deterministic nav output.
- Update only the direct callers/producers required by the selected contract
  and compile both targets.

## Out of scope

- Player navigation, terrain push response, collision radius ownership, or
  other Player gameplay constants in `Players*.cpp`/`.h`.
- FrameCollections include ownership, pusher anchoring, or profile counter
  anchoring; those belong to the other three D15 Plans.
- Terrain steering tuning, explosion/wind tuning, terrain tracing, or a new
  runtime configuration system.
- Targets or target acquisition. The approved
  `Documents/Features/Frame/GenericSpatialQueries.md` feature owns that D3
  deletion.
- Compatibility aliases, speculative abstractions, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3: this crosses Engine/game contracts and
affects deterministic NavBuild data, terrain thresholds, and client/server
shared startup. Preserve floating-point operation order, exact current
clearance and threshold results, Clipper tolerances, `/fp:strict` behavior,
`.pack` inputs, derived nav output, phase/order, and allocation behavior. No
covered Engine path may name or require Player constants after the change.

## Coordination

This Plan is independent of the other three D15 seam Plans and owns only the
navigation clearance/threshold contract. The approved `GenericSpatialQueries.md`
feature is the authority for Targets removal and is not a prerequisite or
scope here.

## Acceptance criteria

- The selected generic clearance/threshold contract is recorded before source
  edits, with the current effective `kfPlayerRadius + kfPushMargin` values
  and arithmetic explicitly preserved.
- A scoped source search finds no covered Engine NavBuild/Main include or
  Player-constant reference, while game-owned Player navigation remains intact.
- Client and server compile; terrain startup and NavBuild produce the same
  thresholds, contour inflation, derived nav data, and deterministic traces.
- A second game can provide clearance/threshold values without Player symbols,
  runtime allocation, compatibility aliases, or a new configuration system.

## Notes

This Plan records only the navigation clearance/threshold seam from the
approved D15 boundary. The other three seams are independently executable
Plans.
