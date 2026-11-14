<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:28.521Z","dependsOn":["Documents/Plans/Engine/GuardTargetOnlyFileDeclarations.md"]} -->
# Reduce DifferenceStream.h With /reduce-file

## Context

`Engine/Source/File/DifferenceStream.h` is the oversized-file item from an
analysis of `Engine/Source/File` (the directory, not its subdirectories) at
commit `7dae9a27`. `.agents/scripts/Get-AnalysisManifest.ps1` measured the
header at 5,098 `bt-token-v1` and marked it `reduceFileCandidate: true`.
`/reduce-file` owns the threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of that analysis. It is not an unmet
acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Engine/Source/File/DifferenceStream.h

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

Navigation hints for that run only. The code-quality metrics pass reported
them and nobody judged them, so they are not findings and not work items:

- Five clone groups inside lines 223-391.
- The `DifferenceStreamReader` constructor, lines 208-421.

Both line ranges are as measured at commit `7dae9a27`. The deletion of
`DifferenceStreamReader::Update` since then is below both ranges and did not
move them; the Plan this one depends on will.

Risk tier: Tier 2 (the author's classification). Trigger: a reduction
restructures a header that `Replay.h` and three translation units — one each
in engine Agent, engine Network, and the game's agent commands — include
directly, more than Tier 1's "no public signature exposure" allows. The
`/reduce-file` run derives the tier again from the reduction it selects.
Exposure: the header holds the server replay stream writer and reader
templates and names game frame types, so a reduction preserves behavior and
the replay stream file bytes, keeps each moved span's `BT_SERVER` affinity,
keeps the header out of `Engine.h` (`Engine/Source/File/AGENTS.md`, opening
paragraph), and changes project membership only through `/update-vcxproj`.

## Critical files

- `Engine/Source/File/DifferenceStream.h`
- The four files that include it directly: `Engine/Source/File/Replay.h`,
  `Engine/Source/Agent/Commands/ClientDesyncProbe.cpp`,
  `Engine/Source/Network/Client/ReconcileReplayTick.cpp`, and the server-only
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`,
  which calls `engine::TransferViaStream`. Every other user, such as
  `Engine/Source/File/Replay.cpp`, reaches the header through `Replay.h`.
- The project and filter files of every project that lists the header

## In scope

- `Engine/Source/File/DifferenceStream.h`: the reduction `/reduce-file`
  selects.
- The new files the reduction creates, the include lines of the four direct
  includers above and of any file that reaches a moved declaration only
  through `Replay.h`, and project and filter membership, as far as that
  reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, any change to the replay stream file bytes, and any
  refactor of a function the reduction does not move.
- The navigation hints above as work items of their own.

## Acceptance criteria

1. `/reduce-file` reports `Engine/Source/File/DifferenceStream.h` below its
   threshold, or reports that no qualifying reduction exists, with its
   evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

No mandatory nondirectional coordination.

## Notes

- The metadata dependency is the Plan that edits this header by line range:
  the one that guards the server-only classes with `BT_SERVER`. It runs first
  so the reduction measures and moves the final content.
  `DifferenceStreamReader::Update` was deleted after the measurement in
  `## Context`, so the file is already smaller than that figure. If the
  dependency is also gone when this Plan is claimed, measure the file again
  before reducing: it may already be below the threshold.
