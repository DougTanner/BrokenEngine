<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:42:52.963Z","dependsOn":[]} -->
# Assert the audio-read ownership invariant; remove ChunkReadResult::kFailed

## Context

Found by the in-function phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt, not an unmet acceptance criterion of any change.

`PackChunks::PollChunkRead` (`Engine/Source/File/PackChunks.cpp:583-618`)
returns `ChunkReadResult::kFailed` in two places, and
`PackChunks::CancelChunkRead` has a matching `break`, for states no code
produces:

- lines 595-599: the entry's generation differs from the request's;
- line 604: the entry is neither queued, loading, nor ready (that is, free)
  while the request still holds it;
- lines 686-689: the cancel loop finds the request's generation on an entry
  that is not queued, ready, or loading.

Proof that none is reachable. Every `uiOwnership` store is in
`PackChunks.cpp` (lines 615, 653, 695, 748, 766, 820, 825);
`Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp:351-433` only
loads. While a request holds an index, only that request's own cancel (695)
or the shutdown acknowledgement (766) changes the generation. Line 766 is
reached only through `PackChunkLoader::Stop` (`PackChunkLoader.cpp:44`),
called from `~PackChunks` (line 81) and the loader destructor, after which
no poll can run on the object; `Engine/Source/File/AGENTS.md`
`## Packed Assets` says "File must outlive Audio". A free entry with the
request's generation needs the store at 615, which clears the request in
the next statement (616). Device loss drains through `WaitForLoadersIdle`
and writes no ownership; audio reset cancels through `CancelChunkRead`
(`Engine/Source/File/FileManager.cpp:20-23`). `ChunkReadRequest` is
noncopyable and nonmovable (`FileManager.h:121-124`).

The cost today: if that invariant ever broke, music would end with a
warning (`Engine/Source/Audio/StreamingVoice.cpp:61-68`) instead of the
break being reported.

Unmet criterion: no handling for a state no writer produces
(`.agents/references/cpp-conventions.md`, first bullet); a broken ownership
invariant halts through `ASSERT`.

## Design

Recommended, as one change, because removing the producers alone would
leave a dead consumer:

1. `PollChunkRead`: replace the generation-mismatch return at 595-599 with
   an `ASSERT` that the generations match, and in the not-ready branch at
   602-605 `ASSERT` the state is queued or loading and return `kPending`.
2. `CancelChunkRead`: replace the `break` at 686-689 with an `ASSERT` of
   the same three states.
3. Delete `ChunkReadResult::kFailed` (`FileManager.h:112`).
4. Delete the `kFailed` branch at `StreamingVoice.cpp:61-68`.

Before editing, repeat the search for `uiOwnership` stores and for `kFailed`
users across the repository, including the harness rig and the game
project; if a producer or another consumer exists, stop and report it.

## Critical files

- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/FileManager.h`
- `Engine/Source/Audio/StreamingVoice.cpp`

## In scope

- `PackChunks::PollChunkRead`: the two `kFailed` returns.
- `PackChunks::CancelChunkRead`: the state test and `break` inside the
  loop.
- `ChunkReadResult::kFailed`.
- The `kFailed` branch of the poll loop in `StreamingVoice.cpp`.
- A sentence in `Engine/Source/File/AGENTS.md` or
  `Engine/Source/Audio/AGENTS.md`, only if one names a failed read result.

## Out of scope

- `kRetry`, `kPending`, and `kReady`, and how Audio handles them.
- Every ownership store, `TryReadChunkData`, `TryClaimAudioRead`,
  `AcknowledgeQueuedAudioReads`, and `LoadAudioRead`.
- The sentinel `kiInvalidAudioReadEntry` and the ownership encoding helpers.
- The harness rig.

## Risk tier

Tier 2 — scoped behavior. Trigger: independently owned cross-subsystem
integration — it removes a value from a File enum that Audio consumes, in
`FileManager.h`, which `Engine.h` aggregates — and it changes what happens
when the ownership invariant is broken (halt instead of a warning). The
ownership protocol between the main thread and the loaders is read, not
changed: no store, ordering, or wake is edited.

## Acceptance criteria

- `kFailed` appears nowhere in the repository's own code as a
  `ChunkReadResult`.
- The BrokenEngineSandbox client and server and ModelViewer build without
  errors or new warnings.
- Live, through `/agent-harness`: a client plays streamed music across at
  least one track change, and through one audio reset or device-loss
  recovery if the harness offers one, with no `ASSERT` and no audio error
  in its log. The analysis recorded no harness setup for this; the
  executing session chooses it.

## Coordination

No mandatory nondirectional coordination.
