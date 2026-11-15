<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:12.787Z","dependsOn":[]} -->
# ComputeReturnDestination reads the centred islands itself

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/` unless they start at
the repository root; `FC/` stands for `Frame/Collections/`. Line numbers are
from the commit that added this Plan; locate each site by symbol.

Originating gap: the architecture phase of a deep analysis of `FC/Fighters`,
verified by a second reader. It is not an unmet acceptance criterion of any
change. The false condition: the island-candidate recipe exists at one site.

Each ship type's PostRender `Update` opens with the same nine lines that
build a per-thread workbuffer of island candidates:
`FC/Fighters/Fighters.cpp:560-568`, `FC/Battleships/Battleships.cpp:414-422`,
`FC/Omnis/Omnis.cpp:573-581` (only the leading comment's wording differs in
Omnis). Each candidate is
`(f2WorldPosition.x, f2WorldPosition.y, engine::gpFlightPlanes->Lowest().fHeightMeters, 1)`
for one entry of `engine::CentredIslands(rStaticData.islands)`.

In each file the span has exactly one consumer, `ComputeReturnDestination`
(`Fighters.cpp:681`, `Battleships.cpp:503`, `Omnis.cpp:691`), defined at
`FC/Ships/ShipBehaviours.cpp:789`. That function already receives
`rStaticData`, and its comment at `:798-799` restates the candidate recipe.
`ComputeNavigation` in the same file reads `CentredIslands` directly (`:607`,
`:657`).

The block counts `CentredIslands(...)` but indexes `rStaticData.islands.at(i)`.
That is correct only because `CentredIslands` returns a prefix of the list
(`Engine/Source/Frame/IslandChainPlacement.h`, "indices into it are indices
into the list").

So three writable copies feed one CRC-visible input (return-to-island
steering) of one function that could read its source itself.

## Design

The authoring agent's choice, not a user decision: the smallest change.
`ComputeReturnDestination` reads `engine::CentredIslands(rStaticData.islands)`
itself and loses its `islandCandidates` parameter; the three blocks and the
three arguments are deleted.

Alternative considered and not chosen, also by the authoring agent and not by
the user: an owning type in `FC/Ships/ShipBehaviours.h`, constructed from
`rStaticData` once per `Update` beside `NavRectangleBoard`, that holds the
workbuffer allocation and exposes the candidate span, with
`ComputeReturnDestination` unchanged. A duplicate Plan from an architecture
review of the Battleships collection (added by commit `7dae9a27`) proposed it
and was removed in favour of this one. Its one advantage: the candidates are
formed once per cell per `Update`, where this Plan forms them per row, one
`XMVectorSet` per centred island for each row that calls
`ComputeReturnDestination`. It was not chosen under the root `AGENTS.md` KISS
and YAGNI directives: it adds a type and keeps the allocation, the span, and
the parameter, where this Plan only deletes code, and the per-row cost is
small beside the scan over the same islands that the function already runs
for each such row.

1. `FC/Ships/ShipBehaviours.h`/`.cpp`: remove the
   `std::span<const XMFLOAT4> islandCandidates` parameter. In the body, take
   the centred-islands span; the empty test and the nearest-center scan run
   over it, building each candidate vector in the loop with the recipe above.
   Rewrite the comment at `:798-799`, which describes the removed
   precomputation.
2. Delete the nine-line block in each of the three `Update` functions and
   the `islandCandidates` argument at each call.

Bit identity, which the change must keep:

- The candidate stays `(x, y, Lowest().fHeightMeters, 1)`. An `XMFLOAT4`
  store and load round trip is exact, so building the vector in the loop gives
  the same bits as loading the stored one.
- The scan keeps list order and the strict `<` first-nearest rule, starting
  from the first centred island.
- The distance stays `XMVector3LengthSq`, so the Z term stays.
- Dropping the workbuffer push has no simulation effect.

Risk tier: Tier 2 — scoped behavior. Trigger: a changed signature in the
shared `FC/Ships` header, on code that feeds CRC-visible steering; the change
does not alter what that surface computes. A mistake here would change the
CRC, which is why criterion 3 is required.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`

## In scope

- `ComputeReturnDestination`: its declaration, its parameter list, the
  candidate read and scan at the top of its body, and that comment.
- The island-candidates block at the top of `FightersPostRender::Update`,
  `BattleshipsPostRender::Update`, and `OmnisPostRender::Update`, and the
  `islandCandidates` argument of the one `ComputeReturnDestination` call in
  each.

## Out of scope

- The return-distance hysteresis, the route waypoint call, and every other
  line of `ComputeReturnDestination`.
- `ComputeNavigation` and its own `CentredIslands` reads.
- `CentredIslands` and the island placement list.
- Any change to which island a ship returns to.

## Acceptance criteria

1. Code read: no ship collection source builds island candidates (no
   `islandCandidates` identifier remains in the three `.cpp` files), and
   `ComputeReturnDestination` takes no candidate span.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Harness (`/agent-harness`): the replay determinism acceptance in
   `.agents/skills/agent-harness/references/replay.md`
   `### Replay determinism acceptance`, over a recording in a cell that has
   centred islands and random-spawn ships with no enemy in the cell (so they
   return to an island), reports no CRC mismatch. The originating analysis was
   a code read and records no harness setup, so the executing session builds
   the scene. Recommended stronger form: make the recording on the build
   before the change and play it back on the changed build; the change bumps
   no version, so the recording stays readable.

## Notes

Other Plans from the same analysis edit files this Plan edits. No ordering is
required; whichever lands second merges. Each name is a Plan under
`Documents/Plans/Game/`:

- `Fighters.cpp`: `RemoveShipNavigationDelayColumn.md` and
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` (item 15).
- `Battleships.cpp`: `RemoveShipNavigationDelayColumn.md` and
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` (item 15).
- `Omnis.cpp`: `RemoveShipNavigationDelayColumn.md`.
- `FC/Ships/ShipBehaviours.h` and `.cpp`:
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` (item 15).

Plans from other analyses overlap the same way, with no ordering required:
`Documents/Plans/Game/ShareShipPreCollision.md` edits `PreCollision` in
`Omnis.cpp`, and `Documents/Plans/Game/SyncFighterBeamHalfWidthParameter.md`
edits `Fighters.cpp` and `Omnis.cpp`.

Two Plans from an analysis of the `FC/Ships` directory overlap the same way,
with no ordering required; whichever lands second re-derives its sites from
the landed source.
`Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` (its item 16)
removes the `vecCellCenter` local, a few lines from the island-candidates
block in the three `Update` functions, and edits `ComputeNavigation` in
`FC/Ships/ShipBehaviours.h` and `.cpp`.
`Documents/Plans/Game/ReduceShipBehavioursCpp.md` reduces
`FC/Ships/ShipBehaviours.cpp` and may move `ComputeReturnDestination` to a new
file, in which case this Plan's sites follow the function.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
