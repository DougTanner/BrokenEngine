<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:56.569Z","dependsOn":["Documents/Plans/Game/ShipNavigationDerivesCallerInvariantInputs.md"]} -->
# Reduce ShipBehaviours.cpp With /reduce-file

## Context

`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
is an oversized-file item from a deep analysis of
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships` (the directory,
not its subdirectories) at commit `fe1fb69b`.
`.agents/scripts/Get-AnalysisManifest.ps1` measured the file at 13,418
`bt-token-v1` and marked it `reduceFileCandidate`. `/reduce-file` owns the
threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of the in-function refactoring phase of
that analysis. It is not an unmet acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

Navigation hints for that run only, with line numbers as of commit `fe1fb69b`.
The code-quality metrics pass reported the first two and nobody judged them;
the rest are in-function observations the analysis forwarded without
inspecting this file. None is a finding, a work item, or a reason to split:

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
  `Documents/Plans/Game/RenameArtificialIntelligenceToAi.md` owns that rename.
  The comment at lines 346-352 holds one over-long line.

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add declarations to
`ShipBehaviours.h`, a header shared across the game, more than Tier 1's "no
public signature exposure" allows. The `/reduce-file` run derives the tier
again from the reduction it selects. Exposure: `ShipBehaviours.cpp` holds
PostRender simulation code that feeds the per-tick CRC and is compiled by both
executables, so a reduction preserves behavior and statement order, keeps each
moved span's `BT_CLIENT` or `BT_SERVER` affinity, and changes project
membership only through `/update-vcxproj`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- The project and filter files of every project that compiles
  `ShipBehaviours.cpp`

## In scope

- `ShipBehaviours.cpp`: the reduction `/reduce-file` selects.
- `ShipBehaviours.h`, the new files the reduction creates, and their project
  and filter membership, as far as that reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- The navigation hints above as work items of their own.
- Two neighbours the originating analysis recorded as investigations of their
  own, each with a choice still open for the user: `pGlobalIds` as a simulation
  input outside the CRC
  (`Documents/Investigations/Engine/ResolveShipGlobalIdCrcExclusion.md`), and
  the mode 6 and mode 7 random-draw rule in the folder's `AGENTS.md`
  `## Fleet Navigation Invariants` against the code
  (`Documents/Investigations/Engine/ResolveModeSixSevenRandomDrawRule.md`).

## Acceptance criteria

1. `/reduce-file` reports `ShipBehaviours.cpp` below its threshold, or reports
   that no qualifying reduction exists, with its evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

`Documents/Plans/Game/RenameArtificialIntelligenceToAi.md` renames identifiers
in `ShipBehaviours.cpp` and lists it as one of three critical files its diff
stays inside. If this reduction lands first and moves a renamed function, that
Plan's sites follow the function into its new file.

`Documents/Plans/Game/ShipIslandCandidatesBuiltOnce.md` adds an owning type to
`ShipBehaviours.h` and `ShipBehaviours.cpp`. If this reduction lands first,
that Plan places the type from the landed source; if that Plan lands first,
this reduction treats the type as part of the file it measures.
