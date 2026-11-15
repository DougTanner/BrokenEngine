<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T18:00:15.932Z","dependsOn":["Documents/Plans/Engine/ConsolidatedSmallDeduplications.md"]} -->
# Inline OmnisPostRender::UpdateFacing, Then Reduce Omnis.cpp and ShipBehaviours.cpp

## Context

Paths without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Line numbers are
from the trees each item was written against (named per item); locate by
symbol if they moved.

### OmnisPostRender::UpdateFacing

`OmnisPostRender::UpdateFacing` is a private static member function declared
at `Omnis/Omnis.h:282` under the comment "Defined in OmnisCombat.cpp:"
(`Omnis/Omnis.h:281`) and defined at `Omnis/OmnisCombat.cpp:35-39`. Its body
is one rationale comment and one assignment, of the normalized difference of
`vecLookPosition` and `vecPosition` to `rVecWantedDirection`. It has one
caller, in `OmnisPostRender::Update` (`Omnis/Omnis.cpp:715`), and no
counterpart in Fighters or Battleships; a search of the `Projects`, `Engine`,
and `Common` trees finds the name only at those three sites.
`Documents/C++StyleGuide.txt` rule 49 forbids a function whose implementation
is only one assignment.

Two comments describe `Update` through this function:

- `Omnis/Omnis.h:262`, "per-omni phase work split into helpers below".
  `UpdateFacing` is the only helper below it that `Update` calls; the other
  two (`SpawnBlasters`, `SpawnDeathExplosions`, `Omnis/Omnis.h:284-286`) are
  called from `OmnisPostRender::Spawn`.
- `Omnis/Omnis.cpp:553`, "and the helpers in OmnisCombat.cpp". `UpdateFacing`
  is the only function of `Omnis/OmnisCombat.cpp` that `Update` calls.

Originating gap: the architecture phase (duplication lens) of a findings-only
analysis of the `Omnis` directory, confirmed by an independent verification
read. It is pre-existing debt. Unmet criterion: rule 49 holds for
`UpdateFacing`.

### Omnis.cpp

`Omnis/Omnis.cpp` is an oversized-file item from an analysis of its directory
(not its subdirectories) at commit `106613c6`.
`.agents/scripts/Get-AnalysisManifest.ps1` measured the file at 11,264
`bt-token-v1` and marked it `reduceFileCandidate: true`.

### ShipBehaviours.cpp

`Ships/ShipBehaviours.cpp` is an oversized-file item from a deep analysis of
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships` (the directory,
not its subdirectories) at commit `fe1fb69b`.
`.agents/scripts/Get-AnalysisManifest.ps1` measured the file at 13,418
`bt-token-v1` and marked it `reduceFileCandidate`.

For both files `/reduce-file` owns the threshold and the qualification rules.
These are `/reduce-file` follow-ups: neither file was analyzed inline, and no
reduction boundary has been chosen. Originating gap: the file-size triage of
the in-function refactoring phase of those analyses. Neither is an unmet
acceptance criterion of any change.

## Design

The executing session does these steps in order.

### 1. Inline UpdateFacing

Recommended (the author's recommendation):

- Replace the call at `Omnis/Omnis.cpp:715` with the assignment, writing
  `vecWantedDirection` directly, and carry the rationale comment from
  `Omnis/OmnisCombat.cpp:37` to that site.
- Delete the definition from `Omnis/OmnisCombat.cpp` and the declaration with
  its "Defined in OmnisCombat.cpp:" line from `Omnis/Omnis.h`.
- Correct the two comments the inlining makes false: `Omnis/Omnis.h:262` and
  `Omnis/Omnis.cpp:553`, so neither says `Update` has helpers, in `Omnis.h`
  or in `OmnisCombat.cpp`.

This edit to `OmnisPostRender::Update` comes first, so the `Omnis.cpp` run
measures and plans against the file as it leaves it.

### 2. Reduce Omnis.cpp

run /reduce-file Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp

### 3. Reduce ShipBehaviours.cpp

run /reduce-file Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp

For steps 2 and 3, `/reduce-file` owns the qualification, the boundary
analysis, the reduction plan, and the execution steps. This Plan chooses no
boundary, file name, or move beyond the shared hint below, and the executing
session does not turn either run into a general refactor of its file.

### Shared boundary hint: the Omni-only Ships functions

Evidence for both runs' boundary choice, not a work item; it widens no scope.
Line numbers as of commit `629ced2e`, from the architecture phase (cohesion
lens) of a deep analysis of
`Projects/BrokenEngineSandbox/Source/Frame/Collections` (recursive),
confirmed by an independent verification read:

- `ApplyAcceleratedMovement` (`Ships/ShipBehaviours.cpp:873`),
  `ApplyTerrainPush` (`:888`, with its file-local constants
  `kfTerrainPushVelocity` and `kfMaximumPushVelocity` at lines 21-22), and
  `RegenerateShield` (`:1046`) each have exactly one caller, all in
  `OmnisPostRender::Update` (`Omnis/Omnis.cpp` lines 712, 752, 749).
  `AcceleratedMovementConstants` (`Ships/ShipTypes.h:144`) is instantiated
  only at `Omnis/Omnis.h:58`. `AcquireTarget` (`Ships/ShipQueries.cpp:163`)
  likewise has one caller, at `Omnis/Omnis.cpp:708`.
- Moving the three `ShipBehaviours.cpp` functions next to their caller is a
  pure move that changes no operation order, but it takes only about 30 lines
  out of `ShipBehaviours.cpp`, and moving them into `Omnis.cpp` would work
  against its reduction. If the runs move them, they move once: out of
  `ShipBehaviours.cpp` into the new Omnis file the step 2 reduction creates.
- `ResolveTerrainContact` also has one caller (`Omnis/Omnis.cpp:758`) but
  stays in `ShipBehaviours.cpp`: it shares the file-static
  `TerrainContactNavData` and the start-overlap prefix with
  `ApplyTerrainBounce`.

### Navigation hints for the ShipBehaviours.cpp run

Line numbers as of commit `fe1fb69b`. The code-quality metrics pass reported
the first two and nobody judged them; the rest are in-function observations
the analysis forwarded without inspecting this file. None is a finding, a
work item, or a reason to split:

- `ComputeNavigation` (lines 567-787): cyclomatic complexity 31, nesting
  depth 4.
- Clone ranges at lines 308-312, 557-561, and 806-810, each a short
  best-candidate loop with a different key.
- The footprint-point draw is written twice inside `ComputeNavigation` (lines
  671-676 and 702-709).
- `ResolveTerrainContact` and `ApplyTerrainBounce` share the start-overlap
  resolve-and-log prefix (lines 914-921 and 931-939).
- The cell-centre fallback in `ComputeNavigation` (lines 761-765) is reached
  only by a stored navigation mode above the documented ones, which no
  simulation write in the folder produces.
- The header names `ComputeNavigation`'s parameter `rVecAiDirection` and the
  definition names it differently (line 567);
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` owns that
  rename. The comment at lines 346-352 holds one over-long line.

### Risk

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add declarations to
`Omnis.h`, which the game precompiled header includes, and to
`ShipBehaviours.h`, a header shared across the game, more than Tier 1's "no
public signature exposure" allows. Each `/reduce-file` run derives the tier
again from the reduction it selects.

Invariants and exposure:

- `Omnis.cpp` holds the Omnis simulation tick path and `ShipBehaviours.cpp`
  holds PostRender simulation code that feeds the per-tick CRC; both are
  compiled by both executables. A reduction preserves behavior and statement
  order, keeps each moved span's `BT_CLIENT` or `BT_SERVER` affinity, and
  changes project membership only through `/update-vcxproj`.
- The step 1 inlining has no public signature exposure: `UpdateFacing` is
  declared under `private:` (`Omnis/Omnis.h:280-282`) and has one caller. The
  assignment writes `pVecWantedDirections`, which is in
  `OmnisPostRender::SharedCrcMembers()`, but the expression and its operands
  are unchanged and both executables compile the same source, so no computed
  value changes. No layout, wire, or version change.

## Critical files

- `Omnis/Omnis.h`
- `Omnis/Omnis.cpp`
- `Omnis/OmnisCombat.cpp`
- `Ships/ShipBehaviours.h`
- `Ships/ShipBehaviours.cpp`
- The project and filter files of every project that compiles `Omnis.cpp` or
  `ShipBehaviours.cpp`

## In scope

- `OmnisPostRender::UpdateFacing`: the declaration and the comment line that
  heads it, the definition, and the call site in `OmnisPostRender::Update`;
  and the two comments listed in step 1.
- `Omnis.cpp` and `ShipBehaviours.cpp`: the reductions `/reduce-file` selects.
- `Omnis.h`, `ShipBehaviours.h`, the new files the reductions create, and
  their project and filter membership, as far as those reductions require.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function a reduction does not
  move.
- `OmnisPostRender::PreCollision`, `siCollisionLayerIndex`, and the placement
  comment above the `PreCollision` declaration in `Omnis/Omnis.h`, beyond a
  pure move a reduction selects.
- Any other statement of `OmnisPostRender::Update` beyond step 1.
- Fighters, Battleships, and the other Omnis files, beyond receiving moved
  code.
- The includes of the Omnis files, beyond what a reduction requires.
- The navigation hints above as work items of their own.
- Two neighbours the originating analysis recorded as investigations of their
  own: `pGlobalIds` as a simulation input outside the CRC
  (`Documents/Investigations/Engine/ResolveShipGlobalIdCrcExclusion.md`, whose
  choice `Documents/Plans/Game/ShipCrcTuplesAndOmniTransfer.md` now carries),
  and the mode 6 and mode 7 random-draw rule in the folder's `AGENTS.md`
  `## Fleet Navigation Invariants` against the code, with a choice still open
  for the user
  (`Documents/Investigations/Engine/ResolveModeSixSevenRandomDrawRule.md`).

## Acceptance criteria

1. No declaration, definition, or call of `UpdateFacing` remains, the
   assignment with its rationale comment is at the former call site, and the
   two comments listed in step 1 no longer describe `Update` as having
   helpers; the diff settles this.
2. `/reduce-file` reports `Omnis.cpp` below its threshold, or reports that no
   qualifying reduction exists, with its evidence.
3. `/reduce-file` reports `ShipBehaviours.cpp` below its threshold, or reports
   that no qualifying reduction exists, with its evidence.
4. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server.
5. The verification each `/reduce-file` reduction plan names passes.

## Coordination

The directional prerequisite is in this Plan's metadata:
`Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` renames
identifiers in `ShipBehaviours.cpp` and edits `ComputeReturnDestination` in
`ShipBehaviours.h` and `ShipBehaviours.cpp`, so the `ShipBehaviours.cpp` run
measures the file with those edits in it. No mandatory nondirectional
coordination.
