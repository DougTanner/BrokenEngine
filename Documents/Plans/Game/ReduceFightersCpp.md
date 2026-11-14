<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:29.882Z","dependsOn":["Documents/Plans/Game/RemoveShipNavigationDelayColumn.md","Documents/Plans/Game/ReturnDestinationReadsCentredIslands.md","Documents/Plans/Game/SyncFighterBeamHalfWidthParameter.md"]} -->
# Reduce Fighters.cpp With /reduce-file

## Context

`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
is an oversized-file item from an analysis of
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters` (its
directory, not its subdirectories) at commit `fe1fb69b`.
`.agents/scripts/Get-AnalysisManifest.ps1` measured the file at 10,442
`bt-token-v1` there and marked it `reduceFileCandidate: true`. The session
that authored this Plan then deleted dead code from the file, and a parallel
clean-up (commit `7dae9a27`) shortened `FightersPostRender::Update`.
`.agents/scripts/Measure-Tokens.ps1` measures the file at 10,310 (798 lines)
in the commit that added this Plan, still a candidate. `/reduce-file` owns the
threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of the in-function refactoring phase of
that analysis. It is not an unmet acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

Navigation hint for that run only. The code-quality metrics pass reported it
and nobody judged it, because the file was not analyzed inline, so it is not a
finding and not a work item:

- `FightersPostRender::Update` (lines 550-741): deep nesting.

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add a declaration to
`Fighters.h`, which the game `Pch.h` reaches through `Frame/Frame.h`, more
than Tier 1's "no public signature exposure" allows. The `/reduce-file` run
derives the tier again from the reduction it selects. Exposure:
`Fighters.cpp` holds CRC-visible PostRender simulation code and is compiled by
both executables, so a reduction preserves behavior, keeps each moved span's
`BT_CLIENT` affinity, and changes project membership only through
`/update-vcxproj`. Battleships mirror this collection
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/AGENTS.md`),
which the `/reduce-file` run weighs when it picks a boundary.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `BrokenEngineSandboxServer.vcxproj`, with their `.filters`

## In scope

- `Fighters.cpp`: the reduction `/reduce-file` selects.
- `Fighters.h`, the new files the reduction creates, and their project and
  filter membership, as far as that reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- The navigation hint above as a work item of its own.

## Acceptance criteria

1. `/reduce-file` reports `Fighters.cpp` below its threshold, or reports that
   no qualifying reduction exists, with its evidence.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. The verification the `/reduce-file` reduction plan names passes.

## Notes

The three Plans this one depends on each delete code from `Fighters.cpp`: the
two from the same analysis that do (the island-candidates block and the five
navigation delay lines), and `SyncFighterBeamHalfWidthParameter.md`, from an
analysis of the Omnis directory (the `kfFighterBeamHalfWidth` constant and the
type-index comparison in `SyncFighterBeam`; it also adds a declaration to
`Fighters.h`). The measured size is close to the threshold, so the reduction
is measured after them.

Other Plans from the same analysis edit files this Plan edits, with no edge.
None deletes code from `Fighters.cpp`, so no ordering is required:
`/reduce-file` measures the file as it finds it, and whichever lands second
merges. Each name is a Plan under `Documents/Plans/Game/`:

- `Fighters.cpp`: `ShareShipAvoidTerrain.md` (one call replaced).
- `Fighters.h`, as far as the reduction adds a declaration:
  `IncludeShipGlobalIdsInCrc.md`, `RemoveShipNavigationDelayColumn.md`, and
  `ShareShipAvoidTerrain.md`.
- The two projects and their filters: `ShareShipAvoidTerrain.md` removes two
  files from them.

## Coordination

Directional dependencies are in the metadata line. No mandatory nondirectional
coordination.
