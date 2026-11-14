<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:41:39.821Z","dependsOn":[]} -->
# Declare the audio-read sentinel and ownership encoding once

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt, not an unmet acceptance criterion of any change.

The streaming-audio read identity is declared in more than one place.

The "no entry" value is spelled twice:

- `Engine/Source/File/FileManager.h:131` initializes
  `ChunkReadRequest::miEntryIndex` with
  `static_cast<int64_t>(std::numeric_limits<uint32_t>::max())`.
- `Engine/Source/File/PackChunks.h:84` defines
  `kiInvalidAudioReadEntry = 4'294'967'295i64`.
- `Engine/Source/File/PackChunks.cpp:585, 589, 622, 675, 707` compare one
  against the other. If the two drift, `CancelChunkRead` (lines 675-680)
  indexes `mAudioReadEntries` out of range from `~ChunkReadRequest`
  (`Engine/Source/File/FileManager.cpp:15-23`).

The ownership-word encoding is declared twice:

- `PackChunks.cpp:14, 23-31`: `kuiAudioReadStateMask`, `AudioReadState`,
  `AudioReadGeneration`.
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp:15, 20-28`: the
  same three, copied. The rig is client Debug only (line 5) and is allowed to
  inspect Pack state (`Engine/Source/File/AGENTS.md` `## Packed Assets`). A
  drift here only corrupts a developer tool's readout.

Unmet criteria: the no-entry value is declared once; the audio-read ownership
encoding is declared once.

## Design

Recommended:

1. Declare one constant for the no-entry value inside `ChunkReadRequest`
   (`FileManager.h:128-132`; `PackChunks` is its friend). Use it for the
   `miEntryIndex` initializer and at the five `PackChunks.cpp` sites, and
   delete `PackChunks.h:83-84` with the access label that existed only for
   it.
2. Move `kuiAudioReadStateMask`, `AudioReadState`, and `AudioReadGeneration`
   into the `BT_CLIENT` block of `PackChunks.h` (lines 9-28), next to
   `AudioChunkReadState`, and delete both copies. Keep the two
   `static_assert`s and `PackAudioReadOwnership` in `PackChunks.cpp`, which
   is their only user.

Rationale for the header home: `PackChunks.h` is the one header both users
already include, and it stays outside `Engine.h`.

## Critical files

- `Engine/Source/File/FileManager.h`
- `Engine/Source/File/PackChunks.h`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp`

## In scope

- The `ChunkReadRequest::miEntryIndex` initializer and a new constant beside
  it.
- `PackChunks::kiInvalidAudioReadEntry` and its five uses.
- `kuiAudioReadStateMask`, `AudioReadState`, and `AudioReadGeneration` in
  `PackChunks.cpp` and `AudioStreamingHarnessRig.cpp`.

## Out of scope

- The value of the sentinel, the mask, or the shift.
- The rig's own hold-token masks (`kuiHoldStateMask`, `kuiHoldOwnerMask`,
  `kuiHoldGenerationShift`).
- Any ownership state transition.
- `PollChunkRead`'s `kFailed` returns.

## Risk tier

Tier 2 — scoped behavior. Trigger: independently owned cross-subsystem
integration — File declarations consumed by the Agent harness rig — and a
declaration added to `FileManager.h`, which `Engine.h` aggregates. Every
value is unchanged, so no runtime behavior, threading protocol, or format
changes. The aggregated header edit rebuilds both precompiled headers once.

## Acceptance criteria

- The no-entry value has one definition, and
  `kuiAudioReadStateMask`, `AudioReadState`, and `AudioReadGeneration` each
  have one definition under `Engine/Source/`.
- The BrokenEngineSandbox client in Debug and Release, the server, and
  ModelViewer build without errors or new warnings.

## Coordination

No mandatory nondirectional coordination.
