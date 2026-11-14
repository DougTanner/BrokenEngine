<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:51.777Z","dependsOn":[]} -->
# Derive caller-invariant inputs inside the shared ship navigation function

## Context

Paths are relative to `Projects/BrokenEngineSandbox/Source/`; `Ships/`,
`Fighters/`, `Omnis/`, and `Battleships/` stand for the folders of those names
under `Frame/Collections/`. Line numbers are as of commit `fe1fb69b`.

The shared function `ComputeNavigation` (`Ships/ShipBehaviours.h:133`, defined
at `Ships/ShipBehaviours.cpp:567`) takes three parameters that all three ship
scripts pass identically, with no caller choice in them. They are the same at
every call site (`Fighters.cpp:644`, `Battleships.cpp:469`, `Omnis.cpp:662`):

- `vecCellCenter` is the same expression in each script (`Fighters.cpp:577`,
  `Battleships.cpp:425`, `Omnis.cpp:571`) and has no other use there.
- `deltaTime` is built from `rFrame.interpolate.fDeltaTime` in each script
  (`Fighters.cpp:561`, `Battleships.cpp:411`, `Omnis.cpp:561`), and `rFrame` is
  already a parameter. The scripts keep using their local elsewhere.
- `flagshipGlobalId` is `rPrevious.pFlagshipGlobalIds[i]` at all three calls,
  while the function reads the row's `pGlobalIds` itself through the locator
  (`ShipBehaviours.cpp:569-572`).

Originating gap: a cohesion finding of the architecture phase of a deep
analysis of `Ships/` (the directory, not its subdirectories) at commit
`fe1fb69b`. It is pre-existing debt, not an unmet acceptance criterion of any
change. The verifying review accepted it with a caveat, recorded under
`## Design`, and rated its benefit marginal, so this Plan is low priority: the
values are passed only because the callers hoist them, and the saving is
trivial. Nothing was built or run.

## Design

Recommended: remove `vecCellCenter`, `deltaTime`, and `flagshipGlobalId` from
`ComputeNavigation` and derive them inside:

- the cell centre from the same expression the scripts use today, with the
  W=1 rationale comment the scripts carry on that local (`Omnis.cpp:568-570`
  and its counterparts) moved with it;
- the tick delta from `rFrame.interpolate.fDeltaTime`;
- the flagship ID from the row's `pFlagshipGlobalIds` in `rPreviousFrame`,
  through the locator.

Caveat the executing session preserves: the flagship ID is read from
`rPreviousFrame`, the frame the callers read it from. The existing `pGlobalIds`
read beside it is from the current `rFrame`; `pFlagshipGlobalIds` is equal in
both frames only because it is written at Spawn alone, so a current-frame read
would rest on that. The author's recommendation is the previous-frame read
because it is exactly the value passed today; if the executing session finds
that read cannot be made exactly equal, `flagshipGlobalId` stays a parameter
and only the other two go.

No value or evaluation order changes: both locals are tick constants and no
caller differs. `vecPosition`, `orders`, `fleetWantedCoordinate`, the pending
ticks, and the five in/out references stay as they are, because they carry real
per-caller variation or follow the load-previous, store-current rule. The
three ship scripts change together, so Fighters and Battleships stay the
parallel pair `Battleships/AGENTS.md` (opening paragraph) describes.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`

## In scope

- `ComputeNavigation`: declaration, definition parameter list, and the derived
  locals at the top of its body.
- In each ship script's PostRender Update: the `vecCellCenter` local and its
  comment, and the arguments of the one `ComputeNavigation` call.

## Out of scope

- The island candidate list each ship script builds and passes to
  `ComputeReturnDestination`, which
  `Documents/Plans/Game/ReturnDestinationReadsCentredIslands.md` owns.
- Any change to the navigation modes, random draws, arrival checks, or the
  path-query cadence in `ComputeNavigation`.
- The other `ComputeNavigation` parameters.
- The in-function repeats inside `ComputeNavigation` (the footprint-point draw
  written twice, the cell-centre fallback branch): they belong to the
  `/reduce-file` run on `ShipBehaviours.cpp`.
- The definition's `rVecArtificialIntelligenceDirection` parameter name, which
  `Documents/Plans/Game/RenameArtificialIntelligenceToAi.md` owns.
- Two neighbours the originating analysis recorded as investigations of their
  own: `pGlobalIds` as a simulation input outside the CRC
  (`Documents/Investigations/Engine/ResolveShipGlobalIdCrcExclusion.md`, whose
  choice `Documents/Plans/Game/IncludeShipGlobalIdsInCrc.md` now carries), and
  the mode 6 and mode 7 random-draw rule in `Ships/AGENTS.md`
  `## Fleet Navigation Invariants` against the code, with a choice still open
  for the user
  (`Documents/Investigations/Engine/ResolveModeSixSevenRandomDrawRule.md`).

## Acceptance criteria

1. `ComputeNavigation` has no `vecCellCenter` or `deltaTime` parameter, and no
   `flagshipGlobalId` parameter unless the fallback under `## Design` applied,
   which the executing session reports with its reason.
2. Where the diff reads the flagship ID inside `ComputeNavigation`, it reads it
   from `rPreviousFrame`.
3. The BrokenEngineSandbox client and server build.
4. A replay determinism check through `/agent-harness` reports no CRC
   mismatch, in a run where a fleet ship of each of the three types reaches
   its `ComputeNavigation` call. A fleet Fighter reaches it only in Patrol
   (`ShipOrders::kSeekAndDestroy` clear) on a tick its flagship is not in its
   cell, so the run includes such a Fighter. The originating analysis ran
   nothing, so it records no harness setup.

## Coordination

`Documents/Plans/Game/RenameArtificialIntelligenceToAi.md` renames a parameter
on the `ComputeNavigation` definition line, which this Plan also edits.
Whichever lands second re-derives its sites from the landed source. That Plan
also renames identifiers on the `ComputeArtificialIntelligenceSteering` call
line, which this Plan does not edit.

`Documents/Plans/Game/ReturnDestinationReadsCentredIslands.md` removes the
`islandCandidates` parameter of `ComputeReturnDestination` in
`ShipBehaviours.h` and `ShipBehaviours.cpp` and deletes the island candidate
block in the three PostRender `Update` bodies, a few lines from the
`vecCellCenter` local this Plan removes there. The two Plans edit different
lines of the same five files; whichever lands second re-derives its sites from
the landed source.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: the
  signature of a function declared in a header that other game source files
  include changes, on a path that feeds the per-tick CRC, more than Tier 1's
  "no public signature or invariant exposure" allows. Every computed value is
  unchanged, so no determinism/CRC surface changes.
- Exposure: determinism/CRC through the source-frame caveat above. No
  serialization, `.pack`, `kiVersion`, wire, replay-format, threading,
  allocation, or shader exposure.
