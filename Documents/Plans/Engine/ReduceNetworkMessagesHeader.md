<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:40:42.283Z","dependsOn":["Documents/Plans/Engine/RemoveCoordUpdateTickReader.md","Documents/Plans/Engine/RemoveUnreferencedNetworkWireSymbols.md","Documents/Plans/Engine/SeparateNetworkProtocolHeaderResponsibilities.md"]} -->
# Reduce NetworkMessages.h With /reduce-file

## Context

`Engine/Source/Network/NetworkMessages.h` is an oversized-file item from an
analysis of `Engine/Source/Network` (its directory, not its subdirectories)
at commit `10d84ca6`. `.agents/scripts/Get-AnalysisManifest.ps1` measured the
file at 6,332 `bt-token-v1` and marked it `reduceFileCandidate: true`.
`/reduce-file` owns the threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of the in-function refactoring phase of
that analysis. It is not an unmet acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Engine/Source/Network/NetworkMessages.h

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

The dependencies in metadata land first because each changes what the file
holds: two delete unreferenced code from it, and one changes its includes
and keeps the contract rows out of it. The run measures the file as it then
stands, and may find that it no longer qualifies.

Navigation hints for that run only. The code-quality metrics pass reported
these clone groups at the commit above and nobody inspected them, so they are
not findings and not work items; their line numbers are those at commit
`c675b6b2` and predate the dependencies:

- Lines 544-549, 614-619, 631-636, and 714-719.
- Lines 698-702, 749-753, 800-804, and 815-819.
- Lines 525-532 and 680-687.
- Lines 308-312 and 411-415.
- Lines 912-916 and 926-930.

Risk tier: Tier 2 (the author's classification). Trigger: a split
restructures a header that `Engine.h` aggregates and can add a header to
every project, more than Tier 1 allows. The `/reduce-file` run derives the
tier again from the reduction it selects. Exposure: the file holds every
engine packet layout, so a reduction preserves each layout's field order and
sizes exactly, changes no contract row, leaves `kiProtocolVersion` alone, and
changes project membership only through `/update-vcxproj`.

## Critical files

- `Engine/Source/Network/NetworkMessages.h`
- `Engine/Source/Engine.h` and `Engine/Source/Network/NetworkProtocol.h`, as
  includers
- The project and filter files of every project that lists the header

## In scope

- `Engine/Source/Network/NetworkMessages.h`: the reduction `/reduce-file`
  selects.
- The new files the reduction creates, the include lines that reach them,
  and their project and filter membership, as far as that reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior, layout, or wire change, and any refactor of code the
  reduction does not move.
- The navigation hints above as work items of their own.

## Acceptance criteria

1. `/reduce-file` reports `Engine/Source/Network/NetworkMessages.h` below its
   threshold, or reports that no qualifying reduction exists, with its
   evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and server and ModelViewer.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

The directional prerequisites are in metadata. No mandatory nondirectional
coordination.
