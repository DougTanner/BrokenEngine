<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:12:29.147Z","dependsOn":[]} -->
# Reduce TextureManager.cpp With /reduce-file

## Context

`Engine/Source/Graphics/Managers/TextureManager.cpp` is an oversized-file
item from an analysis of `Engine/Source/Graphics/Managers` (its directory, not
its subdirectories) at commit `401e4b75`. The analysis manifest of that
analysis (`.agents/scripts/Get-AnalysisManifest.ps1`) measured the file at
10,014 `bt-token-v1`, over the implementation threshold `/reduce-file` owns.
`/reduce-file` owns the threshold and the qualification rules.

This is a `/reduce-file` follow-up. The file was not analyzed inline, and no
reduction boundary has been chosen.

Originating gap: the file-size triage of that analysis. It is not an unmet
acceptance criterion of any change.

## Design

The executing session does this:

run /reduce-file Engine/Source/Graphics/Managers/TextureManager.cpp

`/reduce-file` owns the qualification, the boundary analysis, the reduction
plan, and the execution steps. This Plan chooses no boundary, file name, or
move, and the executing session does not turn it into a general refactor of
the file.

Risk tier: Tier 2 (the author's classification). Trigger: a file split
restructures one subsystem's translation units and can add a declaration to
`TextureManager.h`, which `Engine.h` aggregates, more than Tier 1's "no public
signature exposure" allows. The `/reduce-file` run derives the tier again from
the reduction it selects. Exposure: `TextureManager.cpp` is `BT_CLIENT` code
holding texture adoption and the acquire command buffer, so a reduction
preserves behavior, keeps the whole-file `BT_CLIENT` guard on every new file,
and changes project membership only through `/update-vcxproj`.

## Critical files

- `Engine/Source/Graphics/Managers/TextureManager.cpp`
- `Engine/Source/Graphics/Managers/TextureManager.h`
- The project and filter files of every project that compiles
  `TextureManager.cpp`

## In scope

- `Engine/Source/Graphics/Managers/TextureManager.cpp`: the reduction
  `/reduce-file` selects.
- `Engine/Source/Graphics/Managers/TextureManager.h`, the new files the
  reduction creates, and their project and filter membership, as far as that
  reduction requires.
- Documentation sync that the moved code requires.

## Out of scope

- Any behavior change, and any refactor of a function the reduction does not
  move.
- The work items of the Plans listed under `## Coordination`.

## Acceptance criteria

1. `/reduce-file` reports `Engine/Source/Graphics/Managers/TextureManager.cpp`
   below its threshold, or reports that no qualifying reduction exists, with
   its evidence.
2. `/compile` builds every project whose membership changed, the
   BrokenEngineSandbox client, and ModelViewer.
3. The verification the `/reduce-file` reduction plan names passes.

## Coordination

These Plans edit regions of `TextureManager.cpp`. None depends on this Plan,
and it depends on none. Each was verified as independent of the size
reduction. Whichever lands second re-derives its regions by symbol from the
landed files:

- `Documents/Plans/Engine/TextureManagerDuplicateAndDeadCode.md` (the two
  detail-size functions, `DetailTextureAspectRatio`, the adoption completion
  in `ProcessPendingTextures` and `AdoptUploadedChunk`, the acquire command
  buffer lookup, and the prefiltered mip count store). The author recommends
  landing that Plan first: a reduction moves functions between files and would
  carry the duplicate and dead code along.
- `Documents/Plans/Engine/TextureUploadManagerCleanup.md` (the frame-signal
  site at `TextureManager.cpp:640-641`).
- `Documents/Plans/Engine/StaticDataDrainInClientRuntime.md` (a comment at
  `TextureManager.cpp:206`).

The two lines cited above are those after the authoring session's later edit
to `TextureManager.cpp`, not those at commit `401e4b75`.
