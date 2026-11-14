<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:42:55.310Z","dependsOn":["Documents/Plans/Engine/GenericAgentCommandHeaderExportsAndPlacement.md"]} -->
# Reduce AgentCommandsClientGeneric.cpp With /reduce-file

## Context

`Engine/Source/Agent/AgentCommandsClientGeneric.cpp` is an oversized-file item
from an analysis of `Engine/Source/Agent` (the directory and its
subdirectories) at commit `4fa6d2d7`.
`.agents/scripts/Get-AnalysisManifest.ps1` measures the file, as the commit
that added this Plan leaves it, at 11,563 `bt-token-v1` and marks it
`reduceFileCandidate: true`. `/reduce-file` owns the threshold and the
qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of the in-function refactoring phase of
that analysis. It is not an unmet acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Engine/Source/Agent/AgentCommandsClientGeneric.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

The prerequisite in this Plan's metadata edits this file first (two function
linkages), so the run measures and plans against the file as that edit leaves
it.

Navigation hint for that run only. Nobody judged it, so it is not a finding
and not a work item: two mirrored capture state machines at lines 25-157 and
242-323 (line numbers from the commit that added this Plan; locate by symbol
if they moved).

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add a declaration to
`AgentCommandsClientGeneric.h`, which a precompiled header aggregates, more
than Tier 1's "no public signature exposure" allows. The `/reduce-file`
run derives the tier again from the reduction it selects. Exposure: the file
is whole-file `BT_CLIENT`, so a reduction preserves behavior, keeps that
guard on every new file, and changes project membership only through
`/update-vcxproj`.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`
- `Engine/Source/Agent/AgentCommandsClientGeneric.h`
- The project and filter files of every project that compiles
  `AgentCommandsClientGeneric.cpp`

## In scope

- `AgentCommandsClientGeneric.cpp`: the reduction `/reduce-file` selects.
- `AgentCommandsClientGeneric.h`, the new files the reduction creates, and
  their project and filter membership, as far as that reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- The navigation hint above as a work item of its own.
- The other files under `Engine/Source/Agent`, beyond receiving moved code.

## Acceptance criteria

1. `/reduce-file` reports `AgentCommandsClientGeneric.cpp` below its
   threshold, or reports that no qualifying reduction exists, with its
   evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

The directional prerequisite is in this Plan's metadata. No mandatory
nondirectional coordination.
