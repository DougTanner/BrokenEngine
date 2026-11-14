<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:33.396Z","dependsOn":[]} -->
# Reduce Client.cpp With /reduce-file

## Context

`Engine/Source/Network/Client/Client.cpp` is an oversized-file item from an
analysis of `Engine/Source/Network/Client` (its directory, not its
subdirectories) at commit `d87ad840`. The analysis manifest of that analysis
(`.agents/scripts/Get-AnalysisManifest.ps1`) measured the file at 10,269
`bt-token-v1`, over the implementation threshold `/reduce-file` owns.
`/reduce-file` owns the threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of that analysis. It is not an unmet
acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Engine/Source/Network/Client/Client.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

Navigation hints for that run only. The code-quality metrics pass reported
them and nobody judged them as work, so they are not findings and not work
items:

- `Client::ServerCoordinateUpdateOrResend` (lines 541-623): deep nesting.
- A clone group at lines 415-419, 483-487, 545-549, and 703-707: the four
  load-generation gates. The analysis considered a shared helper for them and
  dropped it, because a helper cannot perform the caller's `return` and the
  server writes the mirrored gate inline; this Plan does not add one.

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add a declaration to
`Client.h`, which `Engine.h` aggregates, more than Tier 1's "no public
signature exposure" allows. The `/reduce-file` run derives the tier again from
the reduction it selects. Exposure: `Client.cpp` is `BT_CLIENT` code holding
the client's wire dispatch, so a reduction preserves behavior, keeps the
whole-file `BT_CLIENT` guard on every new file, changes no packet layout, and
changes project membership only through `/update-vcxproj`.

## Critical files

- `Engine/Source/Network/Client/Client.cpp`
- `Engine/Source/Network/Client/Client.h`
- The project and filter files of every project that compiles `Client.cpp`

## In scope

- `Engine/Source/Network/Client/Client.cpp`: the reduction `/reduce-file`
  selects.
- `Engine/Source/Network/Client/Client.h`, the new files the reduction
  creates, and their project and filter membership, as far as that reduction
  requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- The navigation hints above as work items of their own, including a shared
  load-generation gate helper.

## Acceptance criteria

1. `/reduce-file` reports `Engine/Source/Network/Client/Client.cpp` below its
   threshold, or reports that no qualifying reduction exists, with its
   evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

These Plans edit regions of `Client.cpp`. None depends on this Plan, and it
depends on none. Whichever lands second re-derives its regions by symbol from
the landed file:

- `Documents/Plans/Engine/RemoveOutOfRangeSubscribeAcceptBranch.md`
  (`Client::ServerSubscribeAccept`).
- `Documents/Plans/Engine/RemoveGuidAssignedCallback.md` (the `Client`
  constructor and the connection-response handler).
