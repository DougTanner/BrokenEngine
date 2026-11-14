<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:41:07.682Z","dependsOn":[]} -->
# Reject compressed eager chunks in ValidateChunkHeader

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt, not an unmet acceptance criterion of any change.

`ValidateChunkHeader` in `Engine/Source/File/PackChunks.cpp` bounds
`ChunkHeader::iSize` in two blocks:

- lines 240-254, only when the `kLz4Compressed` flag is set;
- lines 258-268, only when `common::IsCompressed` is false.

`common::IsCompressed` is true for either the LZ4 or the zlib flag
(`Common/DataFile.h:97-100`), so a chunk flagged `kZlibCompressed` gets
neither bound. Only an audio chunk (line 221) and a model chunk (line 226)
reject compression. An eager Scene, Shader, or Raw chunk carrying the zlib
flag therefore passes validation with an unbounded `iSize` and is published
raw (lines 479-486): eager types have no decode path. Their consumers bound
reads by that header size: `Engine/Source/Graphics/Managers/PipelineManager.cpp`
line 58 (check) and line 71 (`iSpirvSize = iSize - iSpirvOffset`), and
`Engine/Source/Graphics/Objects/ModelPipeline.cpp:91`.

Unmet rule: `Engine/Source/File/AGENTS.md` `## Packed Assets` — "A new publish
path validates first", and corrupt packs halt. Unmet criterion: every
published eager chunk's header size fits its chunk. The LZ4-flag case is
bounded (lines 242-253) but still has no eager decode path.

The two blocks also hold the same two checks with the same message (lines
242-249 and 260-267); all five returns give one string, so their order is not
observable.

## Design

Recommended, as the smallest change that closes the gap:

1. Replace the model-only test at line 226 with one that rejects any
   compression flag when `IsEagerChunk(eExpectedDataType)` is true, with a
   message naming an eager chunk. Rationale: eager chunks are published
   without decoding, so a compressed one is corrupt for every eager type, not
   only Model; this generalizes the existing rule and adds no mechanism.
2. Merge lines 240-268 into one block guarded by
   `kLz4Compressed || !IsCompressed` holding the two shared `iSize` checks,
   plus the LZ4-only `std::in_range<int>` check. Keep the comments' facts,
   reworded for the merged block.
3. Update the sentence in `Engine/Source/File/AGENTS.md` `## Packed Assets`
   that says `ValidateChunkHeader` admits "a model chunk at pack open only
   uncompressed" so it states the rule for every eager type.

Step 2 is only worth doing together with step 1 (it removes one decision and
adds one compound condition); do not land it alone.

## Critical files

- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/AGENTS.md`

## In scope

- `ValidateChunkHeader` (`PackChunks.cpp`): the compression test at line 226
  and the two `iSize` bound blocks at lines 240-268.
- The one model-chunk sentence in `Engine/Source/File/AGENTS.md`
  `## Packed Assets`.

## Out of scope

- The zlib decode branch in `PackChunkLoader::LoadChunk`, the
  `kZlibCompressed` flag, and `DataHeader::kiVersion`: a later Plan selects
  the one compression format that stays by measured engine loading time.
- Lazy chunk validation other than the merged bound block, which keeps its
  current result for every lazy chunk.
- DataPacker, and the eager consumers in Graphics.
- A new harness command or a hand-edited pack to exercise the rejection.

## Risk tier

Tier 2 — scoped behavior. Trigger: it tightens a check inside one unit at an
existing boundary (pack open), with the pack format and the trust unchanged.
Exposure: no `.pack` byte, `kiVersion`, wire, determinism, or threading
change; DataPacker emits no compressed eager chunk, so no shipped pack is
newly rejected.

## Acceptance criteria

- `ValidateChunkHeader` returns a failure reason for an eager data type whose
  header carries either compression flag, and still bounds `iSize` for every
  LZ4 and every uncompressed chunk (code reading: nothing reaches the
  rejection with current packs).
- The BrokenEngineSandbox client and server and ModelViewer build, and the
  client and server both start against the current data build without a pack
  validation failure.

## Coordination

No mandatory nondirectional coordination.
