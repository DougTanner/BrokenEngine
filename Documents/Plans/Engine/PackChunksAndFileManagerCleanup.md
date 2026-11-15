<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:42:20.073Z","dependsOn":[]} -->
# Clean up the pack chunks code and FileManager.h

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. Every
item is pre-existing debt, not an unmet acceptance criterion of any change.
Line references are at that commit; an earlier step below shifts the lines a
later step names in the same file, so re-find each named region by its
content.

### Chunk classifiers defined where used

`Engine/Source/File/FileManager.h:137,140` declare `IsEagerChunk` and
`IsServerChunk` as `constexpr` with no body. Their only definitions are in
`Engine/Source/File/PackChunks.cpp:104-113`.

- `Engine/Source/Profile/ProfileScreens.cpp:118` calls `IsEagerChunk` from
  another translation unit, which includes `File/PackChunks.h` (line 3) and
  so sees only the bodiless declaration.
- `IsServerChunk` is called only at `PackChunks.cpp:302,446`, both under
  `BT_SERVER`.

A `constexpr` function is implicitly inline and must be defined in every
translation unit that uses it; using it without a definition there is
ill-formed, no diagnostic required (C++23 `[basic.def.odr]`, externally
verified during the analysis). The build works today only because the
compiler happens to emit the definition from `PackChunks.cpp`; both projects
build with `/GL` outside Debug. Whether MSVC Release emits that definition in
every case is unresolved and is not needed for this correction.

Unmet criterion: each `constexpr` function is defined in every translation
unit that uses it.

### Compressed eager chunks

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

### Manifest chunk-table offset

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

### Target-only File declarations

Both items share one rule: root `AGENTS.md` `## Client/Server Targets` —
"Guard client/server-only code at the narrowest practical scope".

Server-only replay classes in a header client code includes:

- `Engine/Source/File/DifferenceStream.h` has no `BT_SERVER` guard.
  `DifferenceRecord` (line 16), `DifferenceStreamWriter` (line 22), and
  `DifferenceStreamReader` (line 204) are named outside that header only by
  `Replay.h` and `Replay.cpp`, both whole-file `BT_SERVER`.
- Two client translation units include the header and use only
  `TransferViaStream` (line 9):
  `Engine/Source/Agent/Commands/ClientDesyncProbe.cpp:7,127` and
  `Engine/Source/Network/Client/ReconcileReplayTick.cpp:7,202`.
- Unmet criterion: server-only replay classes are not compiled into client
  translation units.

Client-only texture state compiled into the server:

- `Engine/Source/File/FileManager.h:67-69`: `LazyChunk::vkUploadImage` and
  `vmaAllocation` are unguarded.
- Both `PackChunks::ResetTextureChunkStates` overloads are unguarded
  (`Engine/Source/File/PackChunks.h:58-59`, `PackChunks.cpp:529-579`, of
  which only lines 567-576 are guarded). Their callers are client-only:
  `Engine/Source/Graphics/Graphics.cpp:814` and
  `Engine/Source/Frame/IslandTerrainResidency.cpp:274` (file guard at line
  1). The server skips the Texture pack (`PackChunks.cpp:300-306`).
- Unmet criterion: client-only texture state is `BT_CLIENT`.

### Unused FileManager temp directory

`FileManager` resolves, creates, and exposes a temp directory nothing uses:

- `FileFlags::kTempDirectory` (`Engine/Source/File/FileManager.h:11`) and
  `FileManager::mTempDirectory` (`FileManager.h:173`) appear only there and
  at `Engine/Source/File/FileManager.cpp:95-102,138-140`. No caller in this
  repository passes the flag or reads the member.
- The constructor (`FileManager.cpp:93-103`) still creates
  `%TEMP%\<kGameName>` at every start and throws through
  `FailCreateDirectory` when it cannot, so a launch can fail over a directory
  `FileManager` never reads or writes.

The directory itself has one user by path:
`Engine/Source/Profile/ProfileManager.cpp:75-81` writes `ProfileDump.csv`
there. It computes the path itself, and `common::DiagnosticLog` creates the
parent (`Common/Log/DiagnosticLog.cpp:8`), so it does not depend on
`FileManager`. `Engine/Source/Graphics/Screenshot.cpp:153,407` use
`%TEMP%\Screenshots`, a different directory.

Unmet criterion: `FileManager` resolves only the roots
`Engine/Source/File/AGENTS.md` `## File Contracts` names (the asset data root
and the user AppData root).

## Design

Verification the executing session must do first; it is unverified: no other
game project uses `kTempDirectory` or `mTempDirectory`. The analysis could not
search the private game project's source. Search the primary checkout's
`Projects/Coretide` source for `kTempDirectory` and `mTempDirectory`. If
either is used there, stop and report it instead of deleting them (steps
12-14); the other steps do not depend on that result.

Recommended, in this order:

Chunk classifiers:

1. Move the `IsEagerChunk` body to its declaration at `FileManager.h:137` and
   delete the definition at `PackChunks.cpp:104-107`. `FileManager.h:3`
   already includes `Data/DataTypes.h`, so the body compiles there.
2. Make `IsServerChunk` a file-local `static constexpr` function in
   `PackChunks.cpp` inside `#if defined(BT_SERVER)`, placed before its first
   use, keeping its comment. Delete the declaration and its comment at
   `FileManager.h:138-140`. Rationale: it has no caller outside
   `PackChunks.cpp` and none on the client.
3. Correct the include comment at `Engine/Source/File/PackChunks.h:3`, which
   lists "IsEagerChunk/IsServerChunk decls".

Compressed eager chunks (calls `IsEagerChunk` as step 1 defines it):

4. Replace the model-only test at line 226 with one that rejects any
   compression flag when `IsEagerChunk(eExpectedDataType)` is true, with a
   message naming an eager chunk. Rationale: eager chunks are published
   without decoding, so a compressed one is corrupt for every eager type, not
   only Model; this generalizes the existing rule and adds no mechanism.
5. Merge lines 240-268 into one block guarded by
   `kLz4Compressed || !IsCompressed` holding the two shared `iSize` checks,
   plus the LZ4-only `std::in_range<int>` check. Keep the comments' facts,
   reworded for the merged block. Step 5 is only worth doing together with
   step 4 (it removes one decision and adds one compound condition); do not
   do it alone.
6. Update the sentence in `Engine/Source/File/AGENTS.md` `## Packed Assets`
   that says `ValidateChunkHeader` admits "a model chunk at pack open only
   uncompressed" so it states the rule for every eager type.

Manifest chunk-table offset:

7. Add one `inline constexpr int64_t kiChunkTableOffset` in
   `Common/DataFile.h` after `DataHeader` (the struct at lines 515-533), with
   the same expression, and replace both local definitions
   (`PackChunks.cpp:322` and `DataPacker/Source/Main.cpp:237`) with it.
   Rationale: it is a producer/consumer layout constant, and
   `kiChunkDataOffset` is the precedent for where such a constant lives.
8. Leave the writer's `AlignOutputStream` call as it is; it positions the
   stream and is not a second definition of the value.

Target-only declarations:

9. Wrap the server-only part of `DifferenceStream.h` — from `DifferenceRecord`
   to the end of `DifferenceStreamReader`, lines 16-606 — in
   `#if defined(BT_SERVER)`. `TransferViaStream` stays unguarded and the
   header stays a member of both projects. Rationale: moving
   `TransferViaStream` to another header would be riskier than the debt.
10. Put `LazyChunk::vkUploadImage`, `vmaAllocation`, and their comment under
    `#if defined(BT_CLIENT)`.
11. Put both `ResetTextureChunkStates` declarations, with their comment, and
    both definitions under `#if defined(BT_CLIENT)`, and remove the inner
    guard at `PackChunks.cpp:567-576` that the outer one makes redundant.

The Vulkan and VMA types themselves compile on the server by design
(`Common/ExternalHeaders.h:292-298`); steps 10-11 guard the members, not the
includes.

Temp directory (only after the private game project search above is clean):

12. Delete `FileFlags::kTempDirectory` (`FileManager.h:11`) and
    `mTempDirectory` (`FileManager.h:173`).
13. Delete the temp-directory setup at `FileManager.cpp:94-103`. Keep the
    `pcDirectory` buffer declared at line 93: `GetModuleFileNameW` reuses it
    at line 114.
14. Delete the `kTempDirectory` branch of `GetFilePath`
    (`FileManager.cpp:138-141`).

## Critical files

- `Engine/Source/File/FileManager.h`
- `Engine/Source/File/FileManager.cpp`
- `Engine/Source/File/PackChunks.h`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/DifferenceStream.h`
- `Engine/Source/File/AGENTS.md`
- `Common/DataFile.h`
- `DataPacker/Source/Main.cpp`

## In scope

- `Engine/Source/File/FileManager.h`: the `IsEagerChunk` and `IsServerChunk`
  declarations; `LazyChunk::vkUploadImage` and `LazyChunk::vmaAllocation`;
  `FileFlags::kTempDirectory` and `FileManager::mTempDirectory`.
- `Engine/Source/File/FileManager.cpp`: the temp-directory block in the
  `FileManager` constructor and the `kTempDirectory` branch in
  `FileManager::GetFilePath`.
- `Engine/Source/File/PackChunks.cpp`: the `IsEagerChunk` and `IsServerChunk`
  definitions; in `ValidateChunkHeader`, the compression test at line 226 and
  the two `iSize` bound blocks at lines 240-268; the local
  `kiChunkTableOffset` in `PackChunks::LoadPackFiles` (line 322) and its
  uses; both `PackChunks::ResetTextureChunkStates` definitions and their
  inner guard.
- `Engine/Source/File/PackChunks.h`: the comment at line 3 and both
  `ResetTextureChunkStates` declarations.
- `Engine/Source/File/DifferenceStream.h`: one `BT_SERVER` guard around
  `DifferenceRecord`, `DifferenceStreamWriter`, and `DifferenceStreamReader`.
- Any server-compiled initializer or copy of the two `LazyChunk` members that
  the build shows must move under the same guard.
- `Common/DataFile.h`: one new constant `kiChunkTableOffset` beside
  `DataHeader`.
- `DataPacker/Source/Main.cpp`: the definition at line 237 and its uses.
- The one model-chunk sentence in `Engine/Source/File/AGENTS.md`
  `## Packed Assets`.

## Out of scope

- The classification `IsEagerChunk` or `IsServerChunk` returns.
- Every other declaration in `FileManager.h` not named in `## In scope`.
- `Engine/Source/Profile/ProfileScreens.cpp`: its call stays as written.
- The zlib decode branch in `PackChunkLoader::LoadChunk`, the
  `kZlibCompressed` flag, and `DataHeader::kiVersion`: a later Plan selects
  the one compression format that stays by measured engine loading time.
- Lazy chunk validation other than the merged bound block, which keeps its
  current result for every lazy chunk.
- DataPacker other than the `kiChunkTableOffset` definition and its uses, and
  the eager consumers in Graphics.
- A new harness command or a hand-edited pack to exercise the rejection.
- The value of the manifest chunk-table offset and `DataHeader`: no file byte
  changes, so no version bump.
- `kiChunkDataOffset`, and the `AlignOutputStream` call in the DataPacker
  manifest writer.
- `TransferViaStream`, and the two client files that use it.
- Project and filter membership of any file.
- The Vulkan and VMA includes in `Common/ExternalHeaders.h`.
- `ProfileManager`'s own use of `%TEMP%\<kGameName>` and the screenshot
  directory.
- The AppData and data directory setup, `FailCreateDirectory`, and the
  `GetModuleFileNameW` call.
- Every other `FileFlags` value and their numeric values.

## Risk tier

Tier 2 — scoped behavior. Triggers:

- Declarations change in `FileManager.h`, a header `Engine.h` aggregates:
  `IsServerChunk`, `kTempDirectory`, and `mTempDirectory` leave its public
  surface, and `LazyChunk` loses two members on the server only. Two other
  public File headers change which declarations exist per executable, and a
  declaration is added to `Common/DataFile.h`, shared by DataPacker and the
  engine. All of this is more than Tier 1's "no public signature exposure"
  allows. The header edits rebuild both precompiled headers once.
- One check tightens inside one unit at an existing boundary (pack open):
  `ValidateChunkHeader` rejects every compressed eager chunk.
- One subsystem's runtime behavior changes: a launch no longer creates the
  temp directory and can no longer fail on it.

Invariants:

- No `.pack` or `.manifest` byte, `kiVersion`, pack integrity token, wire,
  determinism, or threading change; the trust policy is unchanged.
- DataPacker emits no compressed eager chunk, so no shipped pack is newly
  rejected.
- `LazyChunk` is not serialized; no other runtime behavior changes on either
  executable.

## Acceptance criteria

- The report records the private game project search for `kTempDirectory`
  and `mTempDirectory` and its result.
- `IsEagerChunk` has its body in `FileManager.h` and no other definition;
  `IsServerChunk` is named only inside `PackChunks.cpp` under `BT_SERVER`.
- `ValidateChunkHeader` returns a failure reason for an eager data type whose
  header carries either compression flag, and still bounds `iSize` for every
  LZ4 and every uncompressed chunk (code reading: nothing reaches the
  rejection with current packs).
- `kiChunkTableOffset` has one definition in the repository's own code.
- On the client, `DifferenceStream.h` declares only `TransferViaStream`; on
  the server, `LazyChunk` has no Vulkan member and
  `ResetTextureChunkStates` does not exist.
- No `kTempDirectory` or `mTempDirectory` remains in the repository.
- DataPacker, the BrokenEngineSandbox client and server, and ModelViewer
  build without errors or new warnings, in Debug and Release.
- The client and server both start against the current data build without a
  pack or manifest validation failure.

## Coordination

No mandatory nondirectional coordination.
