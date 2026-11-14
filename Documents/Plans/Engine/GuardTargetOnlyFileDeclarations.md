<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:41:21.487Z","dependsOn":[]} -->
# Guard the target-only declarations in File headers

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. Both
items are pre-existing debt of low value, not an unmet acceptance criterion
of any change. They share one rule: root `AGENTS.md`
`## Client/Server Targets` — "Guard client/server-only code at the narrowest
practical scope".

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

## Design

Recommended:

1. Wrap the server-only part of `DifferenceStream.h` — from `DifferenceRecord`
   to the end of `DifferenceStreamReader`, lines 16-606 — in
   `#if defined(BT_SERVER)`. `TransferViaStream` stays unguarded and the
   header stays a member of both projects. Rationale: moving
   `TransferViaStream` to another header would be riskier than the debt.
2. Put `LazyChunk::vkUploadImage`, `vmaAllocation`, and their comment under
   `#if defined(BT_CLIENT)`.
3. Put both `ResetTextureChunkStates` declarations, with their comment, and
   both definitions under `#if defined(BT_CLIENT)`, and remove the inner
   guard at `PackChunks.cpp:567-576` that the outer one makes redundant.

The Vulkan and VMA types themselves compile on the server by design
(`Common/ExternalHeaders.h:292-298`); this change guards the members, not
the includes.

## Critical files

- `Engine/Source/File/DifferenceStream.h`
- `Engine/Source/File/FileManager.h`
- `Engine/Source/File/PackChunks.h`
- `Engine/Source/File/PackChunks.cpp`

## In scope

- One `BT_SERVER` guard around `DifferenceRecord`, `DifferenceStreamWriter`,
  and `DifferenceStreamReader` (`DifferenceStream.h`).
- `LazyChunk::vkUploadImage` and `LazyChunk::vmaAllocation`
  (`FileManager.h`).
- Both `PackChunks::ResetTextureChunkStates` overloads: declarations and
  definitions.
- Any server-compiled initializer or copy of the two `LazyChunk` members that
  the build shows must move under the same guard.

## Out of scope

- `TransferViaStream`, and the two client files that use it.
- `IsServerChunk` and `IsEagerChunk`: another Plan owns them.
- Project and filter membership of any file.
- The Vulkan and VMA includes in `Common/ExternalHeaders.h`.
- Any behavior change.

## Risk tier

Tier 2 — scoped behavior. Trigger: it changes which declarations exist per
executable in `FileManager.h`, a header `Engine.h` aggregates, and in two
other public File headers, more than Tier 1's "no public signature exposure"
allows. No runtime behavior changes on either executable; `LazyChunk` loses
two members on the server only, and it is not serialized. The aggregated
header edit rebuilds both precompiled headers once.

## Acceptance criteria

- On the client, `DifferenceStream.h` declares only `TransferViaStream`; on
  the server, `LazyChunk` has no Vulkan member and
  `ResetTextureChunkStates` does not exist.
- The BrokenEngineSandbox client and server and ModelViewer build without
  errors or new warnings.

## Coordination

No mandatory nondirectional coordination.
