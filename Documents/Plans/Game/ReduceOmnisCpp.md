<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:17:05.420Z","dependsOn":["Documents/Plans/Game/InlineOmnisUpdateFacing.md"]} -->
# Reduce Omnis.cpp With /reduce-file

## Context

`Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp` is an
oversized-file item from an analysis of its directory (not its
subdirectories) at commit `106613c6`.
`.agents/scripts/Get-AnalysisManifest.ps1` measured the file at 11,264
`bt-token-v1` and marked it `reduceFileCandidate: true`. `/reduce-file` owns
the threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of the in-function refactoring phase of
that analysis. It is not an unmet acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

The prerequisite in this Plan's metadata edits `OmnisPostRender::Update` in
the file first, so the run measures and plans against the file as that edit
leaves it.

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add a declaration to
`Omnis.h`, which the game precompiled header includes, more than Tier 1's "no
public signature exposure" allows. The `/reduce-file` run derives the tier
again from the reduction it selects. Exposure: `Omnis.cpp` holds the Omnis
simulation tick path and is compiled by both executables, so a reduction
preserves behavior, keeps each moved span's `BT_CLIENT` or `BT_SERVER`
affinity, and changes project membership only through `/update-vcxproj`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
- The project and filter files of every project that compiles `Omnis.cpp`

## In scope

- `Omnis.cpp`: the reduction `/reduce-file` selects.
- `Omnis.h`, the new files the reduction creates, and their project and
  filter membership, as far as that reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- Fighters, Battleships, and the other Omnis files, beyond receiving moved
  code.

## Acceptance criteria

1. `/reduce-file` reports `Omnis.cpp` below its threshold, or reports that no
   qualifying reduction exists, with its evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

The directional prerequisite is in this Plan's metadata. No mandatory
nondirectional coordination.
