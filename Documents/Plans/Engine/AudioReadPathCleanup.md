<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:42:52.963Z","dependsOn":[]} -->
# Clean up the streaming-audio pack read path

## Context

Found by the architecture and in-function phases of an analysis of
`Engine/Source/File` (the directory, not its subdirectories), verified
against commit `7dae9a27`; every line reference below is against that
commit. It is pre-existing debt, not an unmet acceptance criterion of any
change. Three findings share the streaming-audio read path in
`PackChunks.cpp` and `FileManager.h`.

### Audio-read identity declared more than once

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

### Unreachable ChunkReadResult::kFailed

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

### Two loader-thread read routines

Two routines do the same sector-aligned positional read on a loader thread:

- `PackChunkLoader::ReadChunkRange`
  (`Engine/Source/File/PackChunkLoader.cpp:328-401`) takes `iChunkOffset`;
  its one call passes literal 0 (line 280). It discards the `ReadFile` result
  (line 361) and halts through `ASSERT(iCopySize > 0)` (line 365).
- `PackChunks::LoadAudioRead` (`Engine/Source/File/PackChunks.cpp:774-835`)
  recomputes the same offset, prefix, positional read into
  `mpReadBuffers[iThreadIndex]`, and copy at lines 794-816. On a failed or
  short read it calls `FailMissingRequiredAsset` (lines 810, 814), which
  throws `std::runtime_error` under `ScopedExpectedThrows` (lines 154-165).

The throw is the wrong channel. `LoadAudioRead`'s only caller is the loader
thread (`PackChunkLoader.cpp:229`); `LoadingThread` (lines 149-257) and the
thread entry wrapper (`Common/Threading/ThreadLocal.h:97-102`) have no `try`,
and the two catches (`Engine/Source/Main.cpp:85-89`, `:903-907`) are on other
threads. `Engine/Source/File/AGENTS.md` `## File Contracts` allows that catch
"only around `FileManager` construction and eager-load completion", and
`## Packed Assets` says corrupt packs "validate and `ASSERT`". `ASSERT` also
throws (`Common/ErrorUtils.cpp:14-22`), so both paths end the process
uncaught. What the throw actually loses is the callstack diagnostic, which
`Common/Determinism.cpp:147-150` skips for an expected throw, and the
documented channel; its reinstall text reaches no dialog, and the comment at
`PackChunks.cpp:150-153` is false for this caller.

The analysis recorded three smaller findings on the same lines. Sharing the
read makes each of them void, so this Plan does none of them; `## Design`
says why for each:

- `ReadChunkRange`'s `iChunkOffset` is only ever 0 (`PackChunkLoader.h:44`,
  `.cpp:328,333`, call at 280).
- The `bAligned` `else` branch at `PackChunkLoader.cpp:391-394` was reported
  as unreachable for whole-chunk reads: `mpReadBuffers` is `alignas(4096)`
  (`PackChunks.h:142`), and the destination is 64-byte aligned
  (`Common/AlignedMemory.h:28`) plus `iDataCopied`.
- `ASSERT(uiPhysicalSize <= kiReadBufferSize)` at `PackChunks.cpp:801` is a
  constant relation: `iLength` is bounded by
  `sizeof(AudioChunkReadEntry::data)` (line 624) and the prefix is below
  `kiSectorAlignment`.

### Unmet criteria

- The no-entry value is declared once; the audio-read ownership encoding is
  declared once.
- No handling for a state no writer produces
  (`.agents/references/cpp-conventions.md`, first bullet); a broken ownership
  invariant halts through `ASSERT`.
- One loader-thread pack read routine; a loader-thread read failure halts
  through `ASSERT`.

## Design

The steps edit overlapping regions of `PackChunks.cpp`; after an earlier
step, find a later step's site by symbol, since the commit's line numbers
shift.

### A. Declare the audio-read identity once

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

### B. Assert the ownership invariant; remove ChunkReadResult::kFailed

Done as one unit, because removing the producers alone would leave a dead
consumer. Before editing, repeat the search for `uiOwnership` stores and for
`kFailed` users across the repository, including the harness rig and the
game project; if a producer or another consumer exists, stop and report it.

3. `PollChunkRead`: replace the generation-mismatch return at 595-599 with
   an `ASSERT` that the generations match, and in the not-ready branch at
   602-605 `ASSERT` the state is queued or loading and return `kPending`.
4. `CancelChunkRead`: replace the `break` at 686-689 with an `ASSERT` of
   the same three states.
5. Delete `ChunkReadResult::kFailed` (`FileManager.h:112`).
6. Delete the `kFailed` branch at `StreamingVoice.cpp:61-68`.

### C. One loader-thread pack read routine

`LoadAudioRead` reads through `PackChunkLoader::ReadChunkRange`, so the loader
threads have one read routine and a failed read is reported by that routine's
`ASSERT`.

7. Replace the read block at `PackChunks.cpp:794-816` with

   `mLoader.ReadChunkRange(rLazyChunk, iOffset, iLength, rEntry.data.data(), iThreadIndex)`

   (`PackChunks` is a friend of `PackChunkLoader`, `PackChunkLoader.h:22`).
   An audio range is at most 16 KiB (line 624), so it is one loop iteration,
   and audio is stored uncompressed (line 221), so the read takes the
   uncompressed copy path.
8. Delete the locals `eDataType` and `handleFile` at `PackChunks.cpp:791-792`.
   Their only uses are the `ReadFile` call at line 807 and the two
   `FailMissingRequiredAsset` calls at lines 810 and 814, all inside the
   replaced block, and an unreferenced initialized local fails the build
   (warning C4189 under `TreatWarningAsError`). `rLazyChunk` (line 790) stays:
   the new call uses it.

`ReadChunkRange` itself does not change. How it reports failure is the
behavior audio reads now get: it discards the `ReadFile` result (line 361),
and a read that yields no payload bytes halts at `ASSERT(iCopySize > 0)`
(line 365).

The three smaller findings are not done:

- `iChunkOffset` is not removed: the new call passes a non-zero offset, so the
  parameter has a real caller.
- The `bAligned` test and its `else` are not removed: the branch becomes
  reachable, because an audio range starts at any byte of its chunk, so the
  source pointer need not be 16-byte aligned, and
  `AudioChunkReadEntry::data` has no 16-byte guarantee
  (`PackChunks.h:19-26`).
- No `static_assert` replaces the line 801 `ASSERT`: it leaves with the block
  it guarded, and `ReadChunkRange` sizes its own reads.

Accepted cost: when both pointers happen to be 16-byte aligned,
`ReadChunkRange` writes the entry with non-temporal stores (lines 378-395),
and the main thread copies that entry next. That is correct, with a small
cache cost.

Not chosen: keeping `LoadAudioRead`'s own read and turning its two
`FailMissingRequiredAsset` calls into `ASSERT`s, which leaves two read
routines.

## Critical files

- `Engine/Source/File/FileManager.h`
- `Engine/Source/File/PackChunks.h`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/Audio/StreamingVoice.cpp`
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp`

## In scope

- `FileManager.h`: the `ChunkReadRequest::miEntryIndex` initializer and a new
  constant beside it; `ChunkReadResult::kFailed`.
- `PackChunks.h`: `PackChunks::kiInvalidAudioReadEntry` and its access label;
  the new home of `kuiAudioReadStateMask`, `AudioReadState`, and
  `AudioReadGeneration` in the `BT_CLIENT` block.
- `PackChunks.cpp`:
  - the five uses of `kiInvalidAudioReadEntry`;
  - `kuiAudioReadStateMask`, `AudioReadState`, and `AudioReadGeneration`;
  - `PackChunks::PollChunkRead`: the two `kFailed` returns;
  - `PackChunks::CancelChunkRead`: the state test and `break` inside the
    loop;
  - `PackChunks::LoadAudioRead`: the locals at lines 791-792 and the read
    block at lines 794-816.
- `AudioStreamingHarnessRig.cpp`: its copies of `kuiAudioReadStateMask`,
  `AudioReadState`, and `AudioReadGeneration`.
- `StreamingVoice.cpp`: the `kFailed` branch of the poll loop.
- A sentence in `Engine/Source/File/AGENTS.md` or
  `Engine/Source/Audio/AGENTS.md`, only if one names a failed read result.

## Out of scope

- The value of the sentinel, the mask, or the shift.
- The rig's own hold-token masks (`kuiHoldStateMask`, `kuiHoldOwnerMask`,
  `kuiHoldGenerationShift`), and every other part of the harness rig.
- `kRetry`, `kPending`, and `kReady`, and how Audio handles them.
- Every ownership store and state transition, `TryReadChunkData`,
  `TryClaimAudioRead`, and `AcknowledgeQueuedAudioReads`.
- The ownership compare-exchange, the harness rig hooks, the debug hold, and
  the completion notification in `LoadAudioRead` (lines 776-790, 818-834).
- `PackChunkLoader::ReadChunkRange` and its declaration: no edit.
- `FailMissingRequiredAsset` itself and its startup callers.
- The loader queue, wake, and claim logic in `LoadingThread`.
- Any change to what a successful read returns.

## Risk tier

Tier 2 — scoped behavior. Triggers: independently owned cross-subsystem
integration — it removes a value from a File enum that Audio consumes and
moves File declarations the Agent harness rig consumes, in `FileManager.h`,
which `Engine.h` aggregates (the aggregated header edit rebuilds both
precompiled headers once); and one subsystem's runtime behavior — it changes
what happens when the ownership invariant is broken (halt instead of a
warning), which routine reads a streaming-audio range, and how a failed read
halts.

Invariants:

- The ownership protocol between the main thread and the loaders is read,
  not changed: no store, ordering, or wake is edited.
- The values of the sentinel, the mask, and the shift are unchanged.
- The thread that runs the read, the entry ownership protocol, the pack
  format, and the trust placed in the pack are unchanged, so no Tier-3
  surface is altered.

Exposure to state in review: the read runs on a loader thread and writes
Pack-owned result storage the main thread copies after the ready store.

## Acceptance criteria

- The no-entry value has one definition, and
  `kuiAudioReadStateMask`, `AudioReadState`, and `AudioReadGeneration` each
  have one definition under `Engine/Source/`.
- `kFailed` appears nowhere in the repository's own code as a
  `ChunkReadResult`.
- `LoadAudioRead` reads through `ReadChunkRange`; it holds no `ReadFile`
  call, and the diff does not touch `ReadChunkRange`.
- No `FailMissingRequiredAsset` call remains reachable from a loader thread.
  The failed-read path is verified by code reading, since nothing reaches it
  with an intact pack.
- The BrokenEngineSandbox client in Debug and Release, the server, and
  ModelViewer build without errors or new warnings.
- Live, through `/agent-harness`, in one run: a client plays streamed music
  across at least one track change, and through one audio reset or
  device-loss recovery if the harness offers one, with no `ASSERT` and no
  audio warning or error in its log. The analysis recorded no harness setup
  for this; the executing session chooses it.

## Coordination

No mandatory nondirectional coordination.
