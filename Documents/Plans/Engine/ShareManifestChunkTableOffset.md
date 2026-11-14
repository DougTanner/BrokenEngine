<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:41:49.277Z","dependsOn":[]} -->
# Declare the manifest chunk-table offset once

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt of low value, not an unmet acceptance criterion of any
change.

The offset of the chunk table inside a `.manifest` file is defined twice with
the same expression,
`common::RoundUp<int64_t, common::kiAlignmentBytes>(static_cast<int64_t>(sizeof(common::DataHeader)))`:

- the reader, `Engine/Source/File/PackChunks.cpp:322`, as a function-local
  `kiChunkTableOffset`;
- DataPacker, `DataPacker/Source/Main.cpp:237`.

The DataPacker writer reaches the same position a third way, by calling
`AlignOutputStream` after the header (`DataPacker/Source/Main.cpp:480-481`).
The sibling constant `kiChunkDataOffset` already lives once in
`Common/DataFile.h:513`. Both copies derive from `sizeof(DataHeader)`, so a
drift needs a hand edit of one of them.

Unmet criterion: the manifest chunk-table offset is declared once.

## Design

Recommended: add one `inline constexpr int64_t kiChunkTableOffset` in
`Common/DataFile.h` after `DataHeader` (the struct at lines 515-533), with
the same expression, and replace both local definitions with it. Rationale:
it is a producer/consumer layout constant, and `kiChunkDataOffset` is the
precedent for where such a constant lives.

Leave the writer's `AlignOutputStream` call as it is; it positions the
stream and is not a second definition of the value.

## Critical files

- `Common/DataFile.h`
- `Engine/Source/File/PackChunks.cpp`
- `DataPacker/Source/Main.cpp`

## In scope

- One new constant beside `DataHeader` (`Common/DataFile.h`).
- The local `kiChunkTableOffset` in `PackChunks::LoadPackFiles`
  (`PackChunks.cpp:322`) and its uses.
- The definition at `DataPacker/Source/Main.cpp:237` and its uses.

## Out of scope

- The value of the offset, `DataHeader`, and `DataHeader::kiVersion`: no
  file byte changes, so no version bump.
- `kiChunkDataOffset`.
- The `AlignOutputStream` call in the DataPacker manifest writer.

## Risk tier

Tier 2 — scoped behavior. Trigger: it adds a declaration to a `Common`
header shared by DataPacker and the engine, more than Tier 1's "no public
signature exposure" allows. The value is unchanged, so the `.manifest`
layout, pack integrity token, and every reader result are unchanged.

## Acceptance criteria

- `kiChunkTableOffset` has one definition in the repository's own code.
- DataPacker, the BrokenEngineSandbox client and server, and ModelViewer
  build without errors or new warnings.
- The client and server start against the current data build without a
  manifest validation failure.

## Coordination

No mandatory nondirectional coordination.
