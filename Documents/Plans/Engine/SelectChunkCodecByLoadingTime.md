<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:54:15.198Z","dependsOn":["Documents/Plans/Engine/PackChunksAndFileManagerCleanup.md"]} -->
# Select the chunk compression format by measured engine loading time

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt, not an unmet acceptance criterion of any change.

The pack format names two compression flags, but only one has a producer:

- DataPacker compresses every final texture chunk with `Lz4Compress` and sets
  only `kLz4Compressed` (`DataPacker/Source/ExportJobs/ExportTexture.cpp`,
  lines 104-105, 186-187, 231-232, and 252-253). No other export job sets a
  compression flag.
- `kZlibCompressed` appears only at its enum value and comment
  (`Common/DataFile.h:88,94-99`) and in the comment at
  `Engine/Source/File/PackChunkLoader.cpp:286`.
- The engine decodes both: the LZ4 branch at `PackChunkLoader.cpp:287-298` and
  the zlib branch at lines 299-304, the engine's only `uncompress(` call.
  `ValidateChunkHeader` bounds `iSize` for LZ4 and carves zlib out of its size
  bounds (`Engine/Source/File/PackChunks.cpp:239-257`).

Root `AGENTS.md` `## Directives` asks for one current format. The comments at
`Common/DataFile.h:94-95` and `PackChunkLoader.cpp:286` keep zlib legal on
purpose. The user settled which one stays by a rule instead of by name:
measure engine loading time with LZ4-compressed and with zlib-compressed
chunks, select the one that loads faster, and keep only that one.

What is still open is the measurement itself, which is this Plan's
investigation portion: no zlib-compressed pack exists today, and no recorded
procedure times engine loading. Facts the measurement starts from:

- `ZlibCompress` already exists beside `Lz4Compress`
  (`DataPacker/Source/ExportJobs/Texture/Texture.cpp:705-725`); DataPacker
  uses it for texture intermediates.
- Cached texture chunks are named by `ExportTexture::kiVersion`
  (`DataPacker/Source/ExportJobs/ExportTexture.h:15`), so a codec change with
  the version unchanged republishes the cached LZ4 chunks.
- Only texture chunks are compressed, and texture chunks are lazy, loaded and
  decompressed on the loader threads (`PackChunkLoader::LoadChunk`). The
  client waits for its priority textures before its first render
  (`Engine/Source/AGENTS.md` `## Startup and Main Loop`).

Unmet criterion: the pack format has one compression format, the one with the
lower measured engine loading time.

## Design

The executing session measures first, then removes the format that lost.

Measurement (the investigation portion):

1. Produce a zlib-compressed data build. As a temporary verification edit,
   switch the four `ExportTexture.cpp` sites to `ZlibCompress` and
   `kZlibCompressed`, change `ExportTexture::kiVersion` so the texture chunks
   re-export, and build the data through `/compile`. The engine needs no edit
   to read it: the zlib decode branch exists.
2. Measure engine loading time against the LZ4 data build and against the zlib
   data build, with the same engine build, machine, and procedure for both.
   The session chooses and records the procedure: what interval counts as
   engine loading time, how it is timed, how many runs, and how disk caching
   is kept equal between the two. The author recommends timing the client
   from process start to its first rendered frame, since that interval holds
   the priority-texture wait, and launching through `/agent-harness`.
3. Use each helper as it is today (`Lz4Compress`, `ZlibCompress`); do not
   tune either one's level for the measurement.
4. The format with the lower measured engine loading time is selected. That
   is the whole selection rule: pack size, export time, and code size do not
   enter it. If the two figures cannot be told apart, report both and stop;
   do not pick by another criterion.

Then keep only the selected format. The author recommends these boundaries.

If LZ4 is selected:

1. Revert the temporary DataPacker edit.
2. `ValidateChunkHeader`: reject a header carrying `kZlibCompressed` for
   every data type, and remove the zlib wording in its comments.
3. `PackChunkLoader::LoadChunk`: delete the zlib branch and the "both
   codecs" and "stays a legal decode branch" comment text; the LZ4 branch
   becomes unconditional under `bCompressed`.
4. Remove `kZlibCompressed` and its comment text from `Common/DataFile.h`;
   `IsCompressed` then tests one flag.
5. Reword the scratch-buffer comment at
   `Engine/Source/File/PackChunks.h:128` ("LZ4 or zlib").

If zlib is selected:

1. Keep the four `ExportTexture.cpp` sites on `ZlibCompress` and
   `kZlibCompressed`, bump `ExportTexture::kiVersion` by its ordinary rule
   in place of the temporary change, and delete `Lz4Compress`
   (`Texture.cpp`, its declaration, and the comment at
   `ExportTexture.cpp:156` that names it) once nothing calls it.
2. `ValidateChunkHeader`: reject a header carrying `kLz4Compressed`, and
   delete the LZ4 `iSize` bounds with no replacement, so the `iSize` bound
   block the dependency leaves is guarded by `!IsCompressed` alone. A zlib
   chunk needs no `iSize` bound: its decode takes the on-disk extent and
   `rLazyChunk.iDataSize` (`PackChunkLoader.cpp:301-303`), which
   `ValidateChunkLocation` and the `iUncompressedSize` checks
   (`PackChunks.cpp:281-291`) already bound.
3. `PackChunkLoader::LoadChunk`: delete the LZ4 branch and its comments; the
   zlib branch becomes unconditional under `bCompressed`.
4. Remove `kLz4Compressed` and its comment text from `Common/DataFile.h`.
5. Reword the comment at `PackChunks.h:128`, and the sentences that name LZ4
   as the texture chunk format: "Every final texture chunk is
   LZ4-compressed" in `DataPacker/Source/ExportJobs/AGENTS.md`
   `## Matching and Routing`, and "Final texture chunks use LZ4" in
   `DataPacker/Source/ExportJobs/Texture/AGENTS.md`
   `## Intermediate Contract`.

In either case bump `DataHeader::kiVersion` (`Common/DataFile.h:525`): root
`AGENTS.md` `## Directives` asks for a bump of a changed format's owning
version, and the chunk flag set is part of the pack format. With LZ4 selected
no stored byte changes, so the bump only forces a re-export.

The LZ4 library stays linked in either case: Network uses it
(`Engine/Source/Network/NetworkSerialization.cpp`). Check whether the engine
still links zlib for another consumer before touching any project file;
removing a library dependency is not part of this Plan.

## Critical files

- `Common/DataFile.h`
- `DataPacker/Source/ExportJobs/ExportTexture.cpp`
- `DataPacker/Source/ExportJobs/ExportTexture.h`
- `DataPacker/Source/ExportJobs/Texture/Texture.cpp`
- `Engine/Source/File/PackChunkLoader.cpp`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/PackChunks.h`

## In scope

- The investigation: producing the zlib-compressed data build and measuring
  engine loading time with each format. Its temporary edits do not land.
- `ChunkFlags::kZlibCompressed`, `ChunkFlags::kLz4Compressed`, `IsCompressed`,
  and their comments (`Common/DataFile.h`): the losing flag is removed.
- `DataHeader::kiVersion`.
- The losing format's branch and its comments in
  `PackChunkLoader::LoadChunk`.
- The compression-flag tests, `iSize` bounds, and comments in
  `ValidateChunkHeader`.
- The comment at `PackChunks.h:128`.
- Only if zlib is selected: the four compression sites in
  `ExportTexture.cpp`, `ExportTexture::kiVersion`, `Lz4Compress` and its
  declaration, and the two DataPacker `AGENTS.md` sentences named above.

## Out of scope

- DataPacker's zlib use for texture intermediates and its legacy migration.
- LZ4 use in `Engine/Source/Network`.
- Project files and the zlib and LZ4 libraries themselves.
- Compression of any chunk type that is uncompressed today.
- Tuning either codec's compression level.
- Any other chunk flag or validation rule.

## Risk tier

Tier 3 — invariant/integration. Trigger: serialization and data layout — it
removes a flag from the `.pack` chunk header format that `Common`, DataPacker,
and the engine share, bumps `DataHeader::kiVersion`, which makes every
existing data build unreadable until re-exported, and with zlib selected
changes the stored bytes of every texture chunk.

## Acceptance criteria

- The change's report states the measurement procedure, the measured engine
  loading time for each format, and the format selected by the lower figure.
- `ChunkFlags` holds exactly one compression flag, the selected one; the
  other flag's name and its decode call (`uncompress(` or the chunk
  `LZ4_decompress_safe` in `PackChunkLoader.cpp`) no longer appear under
  `Common/` or `Engine/Source/File`.
- `DataHeader::kiVersion` differs from its value at `7dae9a27`.
- No temporary measurement edit remains in the diff.
- DataPacker, the BrokenEngineSandbox client and server, and ModelViewer
  build; the client and server start against a data build produced by this
  change's DataPacker, and the client renders textures.

## Coordination

No mandatory nondirectional coordination.

## Notes

- The metadata dependency is the Plan that rejects compressed eager chunks in
  `ValidateChunkHeader`. It edits the same lines, so the line numbers above
  for `PackChunks.cpp` move once it lands; the symbols do not.
