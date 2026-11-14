<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:09:59.776Z","dependsOn":[]} -->
# Reduce GameBase.cpp With /reduce-file

## Context

`Engine/Source/GameBase.cpp` is an oversized-file item from an analysis of
`Engine/Source` (its directory, not its subdirectories) at commit `ae681f03`.
`.agents/scripts/Get-AnalysisManifest.ps1` measured the file at 12,397
`bt-token-v1` and marked it `reduceFileCandidate: true`. `/reduce-file` owns
the threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of the in-function refactoring phase of
that analysis. It is not an unmet acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Engine/Source/GameBase.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

Navigation hints for that run only. The code-quality metrics pass reported
them and nobody judged them, so they are not findings and not work items:

- `GameBase::ProcessInput` (lines 50-230): cyclomatic complexity 50.
- `GameBase::ServerUpdate` and `GameBase::HandleDeferredSwapchain`: nesting
  depth 4.
- A clone pair at lines 690-698 and 704-712.

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add a declaration to
`GameBase.h`, which each game's `Pch.h` includes, more than Tier 1's "no
public signature exposure" allows. The `/reduce-file` run derives the tier
again from the reduction it selects. Exposure: `GameBase.cpp` holds the server
tick path and is compiled by both executables, so a reduction preserves
behavior, keeps each moved span's `BT_CLIENT` or `BT_SERVER` affinity, and
changes project membership only through `/update-vcxproj`.

## Critical files

- `Engine/Source/GameBase.cpp`
- `Engine/Source/GameBase.h`
- The project and filter files of every project that compiles `GameBase.cpp`

## In scope

- `Engine/Source/GameBase.cpp`: the reduction `/reduce-file` selects.
- `Engine/Source/GameBase.h`, the new files the reduction creates, and their
  project and filter membership, as far as that reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- The navigation hints above as work items of their own.

## Acceptance criteria

1. `/reduce-file` reports `Engine/Source/GameBase.cpp` below its threshold, or
   reports that no qualifying reduction exists, with its evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server.
3. The verification the `/reduce-file` reduction plan names passes.
