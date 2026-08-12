<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T02:20:35.615Z","dependsOn":[]} -->
# Prune the unreferenced blaster space-drone audio source

## Context

`Projects/BrokenEngineSandbox/Data/Audio/Blaster/New/609840__eminyildirim__space-drone-ambience-7-variation_0.wav`
is still discovered and packed — the generated
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data/Audio.h`
declares `kAudioBlasterNew609840__eminyildirim__spacedroneambience7variation_0wav`
— but no source file references that identifier. A repository-wide search for
`609840` outside generated output and `Temp/` finds only the asset itself and its
attribution sidecar
`609840__eminyildirim__space-drone-ambience-7-variation.wav.txt`. The last
consumer was already commented out and was deleted by the session that recorded
this Plan; removing the asset was outside that session's approved boundary.

Those two files are the only tracked contents of the
`Data/Audio/Blaster/New/` directory.

## Design

Delete both tracked files — the `.wav` and its attribution sidecar, which exists
only to cover that asset, satisfying `Engine/Data/AGENTS.md`'s requirement to
keep licences and attribution with their asset. Audio export discovers sources by
directory walk, so no packer list, enum table, or version constant names the
asset: the next export simply stops emitting its chunk and its generated `kAudio`
constant. No repository-tracked file references the identifier, so nothing else
needs updating.

`ExportAudio`'s version constant is deliberately not bumped: `DataPacker/Source/ExportJobs/AGENTS.md`
requires that bump for policy, repair-order, or export-rate changes, and removing
a source is none of those.

Before deleting, the implementer confirms the identifier is absent from every
tracked source file, including commented-out code, and confirms that no persisted
save, replay, or wire payload stores raw audio enum ordinals — audio constants
are compile-time consumed today, and finding otherwise escalates this Plan
instead of proceeding.

## Critical files

- `Projects/BrokenEngineSandbox/Data/Audio/Blaster/New/609840__eminyildirim__space-drone-ambience-7-variation_0.wav`
- `Projects/BrokenEngineSandbox/Data/Audio/Blaster/New/609840__eminyildirim__space-drone-ambience-7-variation.wav.txt`

## In scope

- Deleting those two tracked files

## Out of scope

- Every other audio source and sidecar, including the rest of the `Blaster` tree
- `DataPacker` code, export policy, audio repair, and version constants
- Generated `Audio.h`, `.pack`, `.manifest`, and cache contents, which are
  regenerated rather than edited
- Blaster runtime code and sound selection

## Risk tier and invariants

Expected Change Workflow Tier 2 — removing a packed asset changes DataPacker
output for one subsystem. Escalate to Tier 3 if the pre-delete check finds any
persisted or wire payload carrying audio enum ordinals. Invariant: every audio
constant still referenced by code must survive export unchanged, and each
surviving asset keeps its attribution sidecar.

## Acceptance criteria

- No tracked file references `609840` or the `kAudioBlasterNew609840...`
  identifier
- A clean data export succeeds and the regenerated `Audio.h` no longer declares
  that constant, while every constant referenced by code is still declared
- Client and server build and run against the regenerated data, with blaster
  audio unchanged
