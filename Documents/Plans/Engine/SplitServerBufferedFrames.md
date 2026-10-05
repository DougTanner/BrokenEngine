<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T23:09:46.189Z","dependsOn":["Documents/Plans/Engine/SimplifyBufferFrameStructuredBinding.md"]} -->
# Split engine Server into Server and ServerBufferedFrames

## Context

`engine::Server` (`Engine/Source/Network/Server/Server.h:176`) defines its members across three files: `Server.cpp` (6,150 bt-token-v1), `ServerReceive.cpp` (5,002), and `ServerSend.cpp` (3,431), measured with `.agents/scripts/Measure-Tokens.ps1` at baseline `81cf299f`. Merging them into one `Server.cpp` gives about 14,583, over the `/reduce-file` `.cpp` threshold of 10,000 (`.agents/skills/reduce-file/references/worker.md` step 2), and that worker's rules forbid spreading one concrete class's member definitions across sibling `.cpp` files to reduce the measured file. The user decided that a class too big to merge is split into several classes, each keeping all its member definitions in one `.cpp`; `struct` types and free functions are exempt.

The code holds two cohesive groups of state and behavior:

- Connection and admission: the ENet host, `mClients`, the pending queues, the per-update admission budget, violation accounting, Hello/subscribe/unsubscribe/resync handling, the load generation, and the control-channel sends (`SendConnectionResponse`, `SendSubscribeAccept`, `BroadcastLoadNotification`, timespeed). `Server/AGENTS.md` `## Session Invariants` describes this group.
- Buffered frames and the per-slot update stream: `mPerCoordinateBufferedFrames`, `miLatestBufferedTick`, `mBufferedFullFrames`, `mFullFramePool`, `mCompressionBuffer`, `mFrameStreamBuffer`/`mFrameStream`, and `mSendScratch`, read and written only by `BufferFrame`, `BufferFullFrame`, `ClearBufferedFrames`, `FindBufferedFrame`, `CompressToBuffer` (`Server.cpp:358-496`), `SendCoordinateFullState`, `SendCoordinateStaticData`, `WriteBufferedFramePacket`, `SendUpdate`, `SendResends`, `UpdateResendLogState` (`ServerSend.cpp:14-91,128-303`), the per-slot ACK application in `ClientAcknowledgementStream` (`ServerReceive.cpp:49-139`, which reads `miLatestBufferedTick` and settles the `iPendingFullStateTick`/`bHoldUpdatesUntilFullStateAck` hold that `SendUpdate` and `SendCoordinateFullState` arm), and the full-frame ring lookup and send in `ClientDebugFrameRequest` (`ServerReceive.cpp:200-251`). `Server/AGENTS.md` `## Buffered State` describes this group; the code's own names for it are `BufferFrame`, `PerCoordBufferedFrame`, `BufferedFullFrame`, `FindBufferedFrame`, and `ClearBufferedFrames`.

The groups meet at three points only: the update stream reads `muiLoadGeneration` and calls `FindClient`; `Server::ClientAcknowledgementStream` hands a parsed message to the ACK application; `Server::ClientDebugFrameRequest` hands an admitted request to the full-frame send.

## Design

The recommended boundary extracts the buffered-frames group into a new concrete class `engine::ServerBufferedFrames` (`Engine/Source/Network/Server/ServerBufferedFrames.h`/`.cpp`), named from the code's existing buffered-frame terms, owned by value by `Server`. This is `/reduce-file` reduction 2 (extract a cohesive stateful responsibility); a receive/send split was rejected because receive handlers and sends both touch the client records and the load generation, while the buffered frames have their own state and a narrow interface. Both resulting `.cpp` files stay under 10,000.

Recommended shape:

1. `ServerBufferedFrames.h` (whole-file `BT_SERVER` guard, like its siblings) declares `PerCoordBufferedFrame`, `BufferedFullFrame`, and `StringAppendStreamBuf`, moved verbatim from `Server.h`, then the class:
   - `explicit ServerBufferedFrames(Server& rServer)`; a `Server& mrServer` back-reference, the established pattern of `ServerSessionRuntime::mrSession`. The constructor sizes `mCompressionBuffer` to `kiMaxCompressedStatusChangeBatchBytes` under `ScopedSuppressAllocationTracking`, moved from `Server::Server`.
   - Public members, names and signatures unchanged: `BufferFrame`, `BufferFullFrame`, `ClearBufferedFrames`, `SendUpdate`, `SendResends`, `SendCoordinateFullState`, `SendCoordinateStaticData`.
   - Two new public members carrying moved bodies: `ApplyAckStream(ClientConnection& rClient, const NetworkMessages::ClientAckStreamMessage& rMessage, int64_t iClientId)` holding the per-slot loop, floor-stall accounting, and timestamp echo update from `ClientAcknowledgementStream` (`ServerReceive.cpp:49-139`), with `kiPendingFullStateWindowTicks` moved beside it; and `SendDebugFrame(ENetPeer* pPeer, int64_t iTick, GridCoord coord)` holding the `if constexpr (kbDebugFrames)` body from `ClientDebugFrameRequest` (`ServerReceive.cpp:200-250`).
   - Private members: `FindBufferedFrame`, `CompressToBuffer`, `WriteBufferedFramePacket`, `UpdateResendLogState`, and the data members listed in `## Context`, moved with their comments.
   - Forward declarations of `Server` and `ClientConnection`; the header includes only what its declarations need.
2. `ServerBufferedFrames.cpp` holds every member definition above, moved verbatim except that `muiLoadGeneration` reads become `mrServer.muiLoadGeneration` and `FindClient` calls become `mrServer.FindClient`.
3. `Server.h` drops the moved types, members, and declarations, includes `Network/Server/ServerBufferedFrames.h`, adds `friend class ServerBufferedFrames;` beside the existing `friend class ServerSessionRuntime;` so the load generation stays private, and adds the private member `ServerBufferedFrames mBufferedFrames {*this};`.
4. `Server.cpp` absorbs every remaining `Server` member definition from `ServerReceive.cpp` and `ServerSend.cpp` (keeping `kDesynchronizationDiagnosticCooldown`), then those two files are deleted. `Server::ClientAcknowledgementStream` keeps the handshaken lookup, the decode, and the size cross-check that records its own violation, then calls `mBufferedFrames.ApplyAckStream(*pClient, message, iClientId)`. `Server::ClientDebugFrameRequest` keeps the lookup, decode, corrupt-tick throw, and cooldown, then calls `mBufferedFrames.SendDebugFrame(pPeer, message.iTick, message.coord)`, so a disabled-build request still starts its cooldown and returns without lookup, compression, or sending.
5. `ServerSessionRuntime.cpp` calls the moved members through `mpServer->mBufferedFrames` (its friend access already reaches private `Server` members).
6. Keep every log message text byte-identical, including the `Server::` prefixes in moved bodies: `Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md` matches the `Server::SendCoordFullState` line as decisive harness evidence.

Recommended implementation order: create the class with moved bodies, repoint `Server` and `ServerSessionRuntime`, merge the remaining `Server` members into `Server.cpp`, delete the two files, then route `/update-vcxproj` for the server project (remove `ServerReceive.cpp`/`ServerSend.cpp`, add `ServerBufferedFrames.cpp`/`.h`, same filter as `Server.cpp`).

Expected sizes: `Server.cpp` about 9,100 bt-token-v1 (from 6,150 plus merged files); `ServerBufferedFrames.cpp` about 5,900; `Server.h` about 2,100; `ServerBufferedFrames.h` about 1,000.

Change Workflow tier: Tier 2 — trigger: a scoped restructure inside the engine Network Server subsystem that moves public `Server` members (`SendCoordinateFullState`, `SendCoordinateStaticData`) to the new class and repoints their only caller, `ServerSessionRuntime`. No Tier-3 surface changes: wire/protocol bytes, packet order, and channels are unchanged; trust-boundary validation is unchanged, because the ack-stream decode, its cross-check, admission gates, the dispatch catch, and `RecordContractViolation` stay in `Server` and the moved ACK code runs on already-decoded values exactly as before; threading is unchanged (all of it stays server main thread); no determinism/CRC, serialization, `.pack`, replay, or save exposure.

## Critical files

- `Engine/Source/Network/Server/Server.h`, `Server.cpp`, `ServerReceive.cpp` (deleted), `ServerSend.cpp` (deleted)
- `Engine/Source/Network/Server/ServerBufferedFrames.h`, `ServerBufferedFrames.cpp` (new)
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` and `.vcxproj.filters` (through `/update-vcxproj`)
- `Engine/Source/Network/Server/AGENTS.md`, `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` (through `/update-claude-docs`)

## In scope

- Moving the members named in `## Design` from `Server` to `ServerBufferedFrames`, verbatim apart from the `mrServer.` qualification, and the two body splits of `Server::ClientAcknowledgementStream` and `Server::ClientDebugFrameRequest` at the points stated.
- Merging all remaining `Server` member definitions into `Server.cpp` and deleting `ServerReceive.cpp` and `ServerSend.cpp`.
- `Server.h` declaration, include, friend, and member changes; the `ServerSessionRuntime.cpp` call-site repointing.
- Server project and filter membership through `/update-vcxproj`.
- `Server/AGENTS.md` intro ownership sentence (name `ServerBufferedFrames` as owner of the update rings and resend state) and the `VisualStudio2026/AGENTS.md` server-only file list line naming `ServerReceive.cpp`/`ServerSend.cpp`.

## Out of scope

- Any change to wire layouts, `kuiProtocolVersion`, admission gates, contract rows, violation accounting, ACK floor rules, full-state hold logic, resend caps, ring sizes, compression, or log text.
- Public members other callers use (`FindClient`, `Receive`, `mClients`, pending queues, `AdmitGamePacket`, `AdvanceLoadGeneration`, `BroadcastLoadNotification`, `BroadcastTimespeedIfChanged`) and their callers in `Projects/` and `Engine/Source/Server/`.
- `ClientConnection`, `ServerTypes.h`, `ServerSessionRuntime.h`, `ServerBroadcaster`, `ServerTransferManager`, `OwnedEntityRegistry`.
- `Documents/Features/Network/ForwardErrorCorrection.md` line references (manual Feature).
- Any other refactor, rename, or cleanup inside the moved bodies.

## Acceptance criteria

- `Server.cpp` and `ServerBufferedFrames.cpp` each measure at most 10,000 bt-token-v1 with `Measure-Tokens.ps1`, and each of `Server` and `ServerBufferedFrames` has all its out-of-line member definitions in its own single `.cpp`.
- `ServerReceive.cpp` and `ServerSend.cpp` no longer exist; `/update-vcxproj` validation passes.
- The server target builds cleanly in every configuration `/compile` builds for it; the client target is unaffected and still builds.
- Live check through `/agent-harness`: a client connects and its subscribed coordinates receive updates, with a `Server::SendCoordFullState` line logged; `engine_packet_fault_fixture` with `truncated` and `size_mismatch` still records one corrupt violation each without ending the server.

## Coordination

No mandatory coordination constraints. `Documents/Plans/Game/SplitCppCorrespondingHeaderOrder.md` lists include-order rows for `ServerReceive.cpp` and `ServerSend.cpp`; this Plan deletes both files, which supersedes those rows, and that Plan already skips a listed path removed before it runs. `ServerBufferedFrames.cpp` has its own corresponding header, so the include-order rule applies to it normally.

## Notes

- `dependsOn` orders the two small Plans that edit moved bodies first, so their recorded paths stay valid: `CoordinatePacketAggregateInitialization.md` (`Server::WriteBufferedFramePacket` in `ServerSend.cpp`) and `SimplifyBufferFrameStructuredBinding.md` (`Server::BufferFrame` in `Server.cpp`).
- Construction order: `mBufferedFrames` now allocates its compression scratch before `Server::Server` creates the ENet host; both are one-time startup allocations under allocation suppression, so this is not observable.
- `StringAppendStreamBuf` is a class whose members are all defined inline in its header; it moves with its only users and needs no `.cpp`.
