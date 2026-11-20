# Eager Chunk Compression

Revisit When: the eager startup wait (`kBootTimerWaitForDataFile`) or the size
of the eager packs becomes a measured problem.

## Context

Scene, Model, Shader, and Raw are the eager types (`IsEagerChunk`,
`Engine/Source/File/FileManager.h`). `PackChunks::LoadPackFiles` loads each
eager pack whole at startup on the eager-load task: it reads the file through
`std::fstream`, so through the Windows file cache, validates every chunk, and
publishes each `EagerChunk` as a pointer straight into that pack buffer. There
is no decoder on this path.

Pack chunk compression follows the chunk type: `IsCompressed`, beside
`IsEagerChunk`, is false for every eager type, and DataPacker compresses
exactly the types it names. The lazy types it names go through the loader's
LZ4 decode into the lazy chunk buffer; Audio is never compressed.

## What compressing an eager type needs

- `IsCompressed` true for the chosen type, and its export job compressing
  every chunk with `Lz4Compress` and setting `iUncompressedSize`, as the
  texture and island jobs do. Bump `DataHeader::kiVersion` and that job's
  `kiVersion`.
- A decode path on the eager-load task into an aligned buffer that lives for
  the process, because eager consumers keep pointers into chunk data. Each
  published `EagerChunk` then points into that buffer, and its `iDataSize` is
  the decompressed extent.
- `ValidateChunkHeader` bounds keyed on the decompressed extent for that type.
  Model readers derive the outline and hardpoint section offsets from `iSize`
  (`ModelHeader::FighterLaunchBaysOffset`), and a scene chunk carries an
  animation section past `iSize`, so those sections must sit inside the
  decompressed payload and be bounded against it, not against the on-disk
  chunk.

## Measuring before deciding

No measurement has been made. Two ways were identified:

- In-engine temporary decode: export the chosen types compressed, add the
  decode path temporarily, and time startup end to end on the eager-load task.
  The largest temporary edit (chunk layout, validation, buffer), all thrown
  away unless kept.
- DataPacker estimate: for each chunk of the chosen types, compress it with
  `Lz4Compress`, time one decompress, and sum per type; the compressed load
  time is the measured uncompressed startup time plus that sum, labelled an
  estimate. No engine edit.

Either way, the eager packs are read through the Windows file cache, so a
warm-up launch before the measured one hides any saving from reading fewer
bytes from disk; only the added decode time shows. Showing a read-time saving
needs a cold-cache launch.

## Scope

- In: compression of one or more eager types, its decode path, its validation
  bounds, and the measurement that decides it.
- Out: Audio (streaming reads payload ranges straight from the pack); the
  codec choice and level tuning; any per-chunk or per-format choice within one
  type.

## Invariants

- One type-keyed engine predicate (`IsCompressed`) decides decompression; the
  DataPacker export jobs must compress exactly the types it names, and nothing
  checks that they do.
- Corrupt eager data still halts at pack open: every bound is validated before
  a pointer into the decoded buffer is published.
- The Model location table CRC still feeds the handshake pack integrity token,
  so client and server must read the same Model pack.

## Acceptance criteria

- The chosen types export compressed and load through the eager decode path;
  the other eager types are unchanged.
- The ship hitbox outlines the server reads from the Model pack at boot and
  every scene animation section read the same values as before.
- The measurement, its method, and its cache caveat are reported, and the user
  decides whether to keep the change.
