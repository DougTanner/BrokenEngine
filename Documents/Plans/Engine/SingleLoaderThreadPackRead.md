<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:41:36.484Z","dependsOn":[]} -->
# One loader-thread pack read routine, failing through ASSERT

## Context

Found by the architecture and in-function phases of an analysis of
`Engine/Source/File` (the directory, not its subdirectories), verified
against commit `7dae9a27`. It is pre-existing debt, not an unmet acceptance
criterion of any change.

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

Unmet criteria: one loader-thread pack read routine; a loader-thread read
failure halts through `ASSERT`.

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

## Design

`LoadAudioRead` reads through `PackChunkLoader::ReadChunkRange`, so the loader
threads have one read routine and a failed read is reported by that routine's
`ASSERT`.

1. Replace the read block at `PackChunks.cpp:794-816` with

   `mLoader.ReadChunkRange(rLazyChunk, iOffset, iLength, rEntry.data.data(), iThreadIndex)`

   (`PackChunks` is a friend of `PackChunkLoader`, `PackChunkLoader.h:22`).
   An audio range is at most 16 KiB (line 624), so it is one loop iteration,
   and audio is stored uncompressed (line 221), so the read takes the
   uncompressed copy path.
2. Delete the locals `eDataType` and `handleFile` at `PackChunks.cpp:791-792`.
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

- `Engine/Source/File/PackChunks.cpp`

## In scope

- `PackChunks::LoadAudioRead`: the locals at lines 791-792 and the read block
  at lines 794-816.

## Out of scope

- The ownership compare-exchange, the harness rig hooks, the debug hold, and
  the completion notification in `LoadAudioRead` (lines 776-790, 818-834).
- `PackChunkLoader::ReadChunkRange` and its declaration: no edit.
- `FailMissingRequiredAsset` itself and its startup callers.
- The loader queue, wake, and claim logic in `LoadingThread`.
- `PollChunkRead`, `TryReadChunkData`, `CancelChunkRead`, and Audio.
- Any change to what a successful read returns.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's runtime behavior — which
routine reads a streaming-audio range and how a failed read halts. The
thread that runs the read, the entry ownership protocol, the pack format,
and the trust placed in the pack are unchanged, so no Tier-3 surface is
altered. Exposure to state in review: the read runs on a loader thread and
writes Pack-owned result storage the main thread copies after the ready
store.

## Acceptance criteria

- `LoadAudioRead` reads through `ReadChunkRange`; it holds no `ReadFile`
  call, and the diff does not touch `ReadChunkRange`.
- No `FailMissingRequiredAsset` call remains reachable from a loader thread.
- The BrokenEngineSandbox client and server and ModelViewer build without
  errors or new warnings.
- Live, through `/agent-harness`: a client plays streamed music across at
  least one track change with no audio warning or error in its log. The
  analysis recorded no harness setup for this; the executing session chooses
  it. The failed-read path is verified by code reading, since nothing
  reaches it with an intact pack.

## Coordination

No mandatory nondirectional coordination.
