<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:41.865Z","dependsOn":[]} -->
# Reduce ClientSessionRuntime.cpp With /reduce-file

## Context

`Engine/Source/Network/Client/ClientSessionRuntime.cpp` is an oversized-file
item from an analysis of `Engine/Source/Network/Client` (its directory, not
its subdirectories) at commit `d87ad840`. The analysis manifest of that
analysis (`.agents/scripts/Get-AnalysisManifest.ps1`) measured the file at
10,692 `bt-token-v1`, over the implementation threshold `/reduce-file` owns.
`/reduce-file` owns the threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of that analysis. It is not an unmet
acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Engine/Source/Network/Client/ClientSessionRuntime.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

Navigation hints for that run only. The code-quality metrics pass reported
them and nobody judged them as work, so they are not findings, not work
items, and never a mandate to split a function:

- `ClientSessionRuntime::ComputeActiveSet` (lines 914-1023): cyclomatic
  complexity 32.
- `ClientSessionRuntime::ApplyReceivedUpdates` (lines 580-664): nesting depth
  4.

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add a declaration to
`ClientSessionRuntime.h`, which `Engine.h` aggregates, more than Tier 1's "no
public signature exposure" allows. The `/reduce-file` run derives the tier
again from the reduction it selects. Exposure: the file is `BT_CLIENT` code
holding the client poll and drain order, subscription and clock state, and
live terrain preparation, so a reduction preserves behavior and the
main-thread ownership those paths have, keeps the whole-file `BT_CLIENT` guard
on every new file, and changes project membership only through
`/update-vcxproj`. Functions that are `static` in this file today (the client
GUID file functions among them) keep internal linkage unless the reduction
plan shows a declaration is required.

## Critical files

- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.h`
- The project and filter files of every project that compiles
  `ClientSessionRuntime.cpp`

## In scope

- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`: the reduction
  `/reduce-file` selects.
- `Engine/Source/Network/Client/ClientSessionRuntime.h`, the new files the
  reduction creates, and their project and filter membership, as far as that
  reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- The navigation hints above as work items of their own.

## Acceptance criteria

1. `/reduce-file` reports
   `Engine/Source/Network/Client/ClientSessionRuntime.cpp` below its
   threshold, or reports that no qualifying reduction exists, with its
   evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

These Plans edit regions of `ClientSessionRuntime.cpp`. None depends on this
Plan, and it depends on none. Whichever lands second re-derives its regions by
symbol from the landed file:

- `Documents/Plans/Engine/StaticDataDrainInClientRuntime.md` (`PollAndDrain`
  and a new private drain member).
- `Documents/Plans/Engine/RemoveGuidAssignedCallback.md` (`Connect`,
  `PollAndDrain`, and `PersistClientGuidToDisk`).
- `Documents/Plans/Engine/ClientConnectFrameTransitionInEngine.md` (the
  connection handler calls in `PollAndDrain` and `Connect`).
