<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T16:10:03.966Z","dependsOn":[]} -->
# Refresh the stale code citations in the two Frame feature documents

## Context

Three citations in `Documents/Features/Frame/` name locations that no longer
match the source they point at. All three were verified against the current
tree:

- `Documents/Features/Frame/FrameRelativePositions.md:30` states that
  "both `TerrainUtils.cpp` helpers are untouched". The traces it means no longer
  live there: `TracePointAgainstTerrain` is now
  `Engine/Source/Frame/IslandTerrain.cpp` and `TracePointToFrameExit` is now an
  inline definition in `Engine/Source/Frame/FrameUtils.h`, moved by
  `Documents/Plans/Frame/TerrainTraceToEngine.md`. `TerrainUtils.cpp` still owns
  only `ComputeAiSteering` and `ComputeTerrainAvoidance`.
- The same line cites `MakeFrameElevationSampler` at
  `IslandTerrain.cpp:439-458`; the function is actually at
  `Engine/Source/Frame/IslandTerrain.cpp:510` (`IslandTerrain::MakeFrameElevationSampler`).
  That drift is pre-existing: the same commit range shows it at `:510` already
  at baseline `f1090e4`.
- `Documents/Features/Frame/SweptShipTerrainCollision.md:5` cites
  `PlayersNavigation.cpp:457-468` for `PlayersPostRender::ApplyTerrainPush`
  (actually
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:515`)
  and `SpaceshipsNavigation.cpp:118-134` for
  `SpaceshipsPostRender::ApplyTerrainBounce` (actually
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsNavigation.cpp:137`).
  Both were already at those current lines at baseline `f1090e4`, so this drift
  is pre-existing and unrelated to the moved traces.

The citations share one cause — the documents record line ranges and owning
files that later code movement invalidated — one boundary (two documentation
files, no code), and one verification method (locate each symbol and compare).

`Documents/Plans/Frame/TerrainTraceToEngine.md`'s `## In scope` limited
documentation updates to "any `Engine/Source/Frame/AGENTS.md` or game
`Frame/AGENTS.md` sentence that names the old owner", so these two feature
documents were outside that change's boundary; the drift in
`SweptShipTerrainCollision.md:5` and `FrameRelativePositions.md:30`'s sampler
range predates it entirely.

## Design

Correct the three citations in place, changing nothing else about either
document's claims:

- In `FrameRelativePositions.md:30`, restate the "untouched" sentence in terms of
  the current owners — the two moved traces now live in
  `Engine/Source/Frame/IslandTerrain.cpp` and `Engine/Source/Frame/FrameUtils.h`
  and are still unaffected by the frame-relative-position work — and repoint the
  `MakeFrameElevationSampler` citation at its current location.
- In `SweptShipTerrainCollision.md:5`, repoint the `ApplyTerrainPush` and
  `ApplyTerrainBounce` citations at their current locations.

Take every replacement line number from the tree at implementation time rather
than from this Plan, because further landings can move them again.

These are ordinary tracked documents; `Documents/Features` stays manually
executed, and nothing here makes either document a scheduler input or changes
what feature it describes.

## Critical files

- `Documents/Features/Frame/FrameRelativePositions.md` (`:30`)
- `Documents/Features/Frame/SweptShipTerrainCollision.md` (`:5`)

## In scope

- The three citations named in `## Context`: the `TerrainUtils.cpp` ownership
  sentence and the `MakeFrameElevationSampler` range in
  `FrameRelativePositions.md:30`, and the `ApplyTerrainPush` and
  `ApplyTerrainBounce` ranges in `SweptShipTerrainCollision.md:5`

## Out of scope

- Any other statement, design decision, or open question in either document
- A sweep of citations elsewhere in `Documents/`
- Any code change, and any change to the behavior either document describes
- Adding metadata to, or otherwise making a scheduler input of, any
  `Documents/Features` document

## Risk tier and invariants

Expected Tier 1 (documentation only). No code, build, determinism, or data
surface is exposed.

## Acceptance criteria

- Each corrected citation names a file and line that currently contains the
  symbol it claims, verified by locating the symbol in the tree
- No other sentence in either document changes

## Notes

Recorded from the `Documents/Plans/Frame/TerrainTraceToEngine.md` session: the
`SweptShipTerrainCollision.md` findings came from that session's documentation
coherence review, and the `FrameRelativePositions.md` findings from verifying
the moved symbols' new owners.
