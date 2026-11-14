<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:52:04.035Z","dependsOn":[]} -->
# Reduce GlobalUniforms.cpp With /reduce-file

## Context

`Engine/Source/Graphics/Render/GlobalUniforms.cpp` is an oversized-file item
from an analysis of `Engine/Source/Graphics/Render` (the directory, not its
subdirectories) at commit `c675b6b2`. `.agents/scripts/Get-AnalysisManifest.ps1`
measured the file at 10,413 `bt-token-v1` and marked it
`reduceFileCandidate: true`, the only such file in that directory.
`/reduce-file` owns the threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of the in-function refactoring phase of
that analysis. It is not an unmet acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Engine/Source/Graphics/Render/GlobalUniforms.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add declarations to
`Engine/Source/Graphics/Render/Render.h`, more than Tier 1's "no public
signature exposure" allows. The `/reduce-file` run derives the tier again
from the reduction it selects. Exposure: the file is client-only and compiled
by BrokenEngineSandbox and ModelViewer, so a reduction preserves behavior,
keeps the whole-file `BT_CLIENT` guard on every new file, and changes project
membership only through `/update-vcxproj`. Render is outside the CRC.

## Critical files

- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/Render.h`
- The project and filter files of every project that compiles
  `GlobalUniforms.cpp`

## In scope

- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`: the reduction
  `/reduce-file` selects.
- `Render.h`, the new files the reduction creates, and their project and
  filter membership, as far as that reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- The findings other Plans record against this file's functions.

## Acceptance criteria

1. `/reduce-file` reports `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
   below its threshold, or reports that no qualifying reduction exists, with
   its evidence.
2. `/compile` builds every project whose membership changed, and the
   BrokenEngineSandbox client and ModelViewer.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
