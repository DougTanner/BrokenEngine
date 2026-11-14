<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:16.672Z","dependsOn":[]} -->
# Share the Fighter and Battleship AvoidTerrain pass

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/` unless they start at
the repository root; `FC/` stands for `Frame/Collections/`. Line numbers are
from the commit that added this Plan; locate each site by symbol.

Originating gap: the architecture phase of a deep analysis of `FC/Fighters`,
verified by a second reader. It is not an unmet acceptance criterion of any
change. The false condition: the terrain-avoidance gate order exists once, and
no signature carries a parameter that never varies.

`FightersPostRender::AvoidTerrain` (`FC/Fighters/FightersNavigation.cpp:17-61`)
and `BattleshipsPostRender::AvoidTerrain`
(`FC/Battleships/BattleshipsNavigation.cpp:17-61`) are the same function:
replacing `Battleship` with `Fighter` in the second file gives the first, line
for line. Each file holds only that function and two threshold constants,
which are equal in both (`:13-14`: an enemy angle of 0.4 and an enemy distance
of 40). The per-row gate order is: dying, arrival grace, the fleet heading
cone, the enemy-proximity skip, terrain avoidance, then the turn-rate ramp.

Both take `iStart` and `iEnd`, and each has one caller that passes the full
range, `0` and the collection's own row count: `FC/Fighters/Fighters.cpp:740`
and `FC/Battleships/Battleships.cpp:557` (declarations at
`FC/Fighters/Fighters.h:305` and `FC/Battleships/Battleships.h:293`). Each
body already holds that collection through `rFrame`, so the pair carries
nothing. `[[maybe_unused]]` sits on `rFrame` and `rPreviousFrame`, which both
bodies use.

The pass writes `rCurrentInterpolate.pfDeltaRotations[i]`
(`FightersNavigation.cpp:59`), which is simulation state in the CRC. Each row
reads only itself and the previous frame.

How `FC/Ships` already shares Fighter and Battleship code: free functions that
take the type's `HeadingMovementConstants` (`kFighterMovement`,
`kBattleshipMovement`), such as `RampHeadingTurnRate`; and file-local
templates over the collection types in `FC/Ships/ShipBehaviours.cpp`, such as
`IsNavRectangleRow`. `ShipBehaviours.cpp` already includes
`FC/Ships/ShipQueries.h`, so it sees both collection types.

Authority tension, named so it is not resolved silently:
`FC/Battleships/AGENTS.md` says Battleships are a copy of Fighters and the two
stay parallel, and root `AGENTS.md` says "Mirrored patterns stay parallel"; it
also says "DRY: reuse existing mechanisms". Neither forbids a shared helper,
and the two scripts already stay parallel by calling the same `FC/Ships` row
helpers. Equal per-type constants are normal in this mirror (the Battleship
explosion constants equal the Fighter ones), so equal thresholds do not show
the two were meant to be one value. The choices recorded in `## Design` settle
both points.

## Design

The authoring agent's record of what the user chose in the session that
authored this Plan:

- The pass is shared: one function in `FC/Ships/ShipBehaviours`, called by
  both collections. One helper called by both keeps the mirror parallel by
  construction and removes a second writable copy of CRC-visible steering.
  Keeping two copies under the mirror rule was the alternative, and it was not
  chosen.
- The two skip thresholds become one shared pair in `ShipBehaviours.cpp`. The
  values are equal today, and a per-type pair would be passed for a difference
  nothing uses. A pair per type was the alternative, and it was not chosen.

The steps are the author's recommended way to carry those choices out:

1. Add one function, named `AvoidTerrain`, to
   `FC/Ships/ShipBehaviours.h`/`.cpp` that runs the pass for one ship type,
   keyed by `ShipType` and that type's `HeadingMovementConstants`, with no
   range and no threshold parameters. Its body is a file-local template over
   the Interpolate and PostRender collection types, as `IsNavRectangleRow` is,
   instantiated for Fighters and Battleships. The four per-type threshold
   constants become one pair beside it, with the same two values. Move the
   expressions verbatim, in the same order, so `/fp:strict` results do not
   change; `ShipTypeClearanceTier` takes the passed type.
2. `FightersPostRender::Update` and `BattleshipsPostRender::Update` call it in
   place of their `AvoidTerrain` call, at the same point (after the row loop).
   Both call sites drop the `0` and row-count arguments: the shared function
   has no `iStart` or `iEnd` and loops from 0 to the `iCount` of the
   collection it reads through `rFrame`, which is the same row set, since
   `Update` adds and removes no row before the call.
3. Delete both `AvoidTerrain` declarations, and delete
   `FightersNavigation.cpp` and `BattleshipsNavigation.cpp`, which then hold
   nothing.
4. Remove the two files from the client and server projects and filters
   through `/update-vcxproj`.
5. Documentation through `/update-claude-docs` where a sentence names the
   moved function or its file.

Invariants: gate order and every expression unchanged; each row still reads
only itself and the previous frame; the pass still runs once per type per
tick after that type's row loop and before Transfer, Destroy, and Spawn; the
comments that give those reasons move with the code.

Risk tier: Tier 2 — scoped behavior. Trigger: a declaration added to the
shared `FC/Ships` header and two removed from the collection headers, more
than Tier 1's "no public signature exposure" allows, with project membership
changes, on code that writes a CRC'd column; the change does not alter what
that surface computes. A mistake would change the CRC, which is why criterion
5 is required.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsNavigation.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `BrokenEngineSandboxServer.vcxproj`, with their `.filters`

## In scope

- `FightersPostRender::AvoidTerrain` and `BattleshipsPostRender::AvoidTerrain`:
  declarations, definitions, their `iStart` and `iEnd` parameters, their four
  threshold constants, and their one call each with its two range arguments.
- One new function, its file-local template, and the shared threshold pair in
  `FC/Ships/ShipBehaviours.h`/`.cpp`.
- Deleting `FightersNavigation.cpp` and `BattleshipsNavigation.cpp` and their
  project and filter entries.
- Documentation sync the move requires.

## Out of scope

- Any behavior change: the gate order, the thresholds' values, the cone test,
  and what an idle row does.
- `OmnisNavigation.cpp` and Omni steering, which have no `AvoidTerrain`.
- `ComputeTerrainAvoidance`, `FindNearestEnemyShip`, `HasLineOfSight`, and
  `RampHeadingTurnRate`.
- Sharing any other mirrored Fighter and Battleship code.

## Acceptance criteria

1. Code read: the gate order exists in one function, which takes no row range
   and no thresholds; neither call site passes a row range; the two thresholds
   exist once; and neither collection declares `AvoidTerrain`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. The repository static checks pass, including project membership.
4. Code read: `FightersNavigation.cpp` and `BattleshipsNavigation.cpp` are gone
   from the tree and from both projects and their filters.
5. Harness (`/agent-harness`): the replay determinism acceptance in
   `.agents/skills/agent-harness/references/replay.md`
   `### Replay determinism acceptance`, over a recording with fleet and
   random-spawn Fighters and Battleships flying near island terrain, reports
   no CRC mismatch. The originating analysis was a code read and records no
   harness setup, so the executing session builds the scene. Recommended
   stronger form: make the recording on the build before the change and play
   it back on the changed build; the change bumps no version, so the recording
   stays readable.

## Notes

A comment in `FightersPostRender::Update` (`FC/Fighters/Fighters.cpp:657-658`)
refers to this pass's cone test by the function name `AvoidTerrain`. The
shared function keeps that name (Design step 1), so the comment stays true and
this Plan does not edit it.

Other Plans from the same analysis edit files this Plan edits. No ordering is
required; whichever lands second merges. Each name is a Plan under
`Documents/Plans/Game/`:

- `Fighters.cpp`: `ReduceFightersCpp.md`,
  `RemoveShipNavigationDelayColumn.md`, and
  `ReturnDestinationReadsCentredIslands.md`.
- `Battleships.cpp`: `RemoveShipNavigationDelayColumn.md` and
  `ReturnDestinationReadsCentredIslands.md`.
- `Fighters.h` and `Battleships.h`: `IncludeShipGlobalIdsInCrc.md` and
  `RemoveShipNavigationDelayColumn.md`.
- `Fighters.h`: `ReduceFightersCpp.md`, as far as its reduction adds a
  declaration.
- `FC/Ships/ShipBehaviours.h` and `.cpp`:
  `ReturnDestinationReadsCentredIslands.md`.
- The two projects and their filters: `ReduceFightersCpp.md`, as far as its
  reduction adds files.

A Plan from another analysis overlaps the same way, with no ordering required:
`Documents/Plans/Game/SyncFighterBeamHalfWidthParameter.md` edits
`Fighters.cpp` and `Fighters.h`.

Two Plans from an analysis of the `FC/Ships` directory overlap the same way,
with no ordering required.
`Documents/Plans/Game/ReduceShipBehavioursCpp.md` reduces
`FC/Ships/ShipBehaviours.cpp`, where this Plan adds a function: if that
reduction lands first, the executing session places the function from the
landed source, and if this Plan lands first, the reduction measures the file
with the function in it.
`Documents/Plans/Game/ShipNavigationDerivesCallerInvariantInputs.md` edits
`ComputeNavigation` in `FC/Ships/ShipBehaviours.h` and `.cpp`, and other lines
of the `Update` bodies in `Fighters.cpp` and `Battleships.cpp`.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
