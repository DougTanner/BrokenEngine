<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-09T13:58:33.736Z","dependsOn":[]} -->
# Decompress Client Full State Only After Classification Commits

## Context

`Client::ServerCoordFullState` (`Engine/Source/Network/Client/ClientReceive.cpp:183-260`) LZ4-decompresses the
full-state payload into a `game::Frame` at
`Engine/Source/Network/Client/ClientReceive.cpp:206` (`DecompressAndReadFrame`), before it calls
`ClassifyFullState(uiSlotIndex, uiEpoch, coord)` at
`Engine/Source/Network/Client/ClientReceive.cpp:214`.

Classification then rejects the packet on several reachable paths that never use the decompressed frame:
`FullStateFlags::kRejectAsGhost` returns at
`Engine/Source/Network/Client/ClientReceive.cpp:216-226`, the missing-`kCommit` case returns at
`Engine/Source/Network/Client/ClientReceive.cpp:228-231`, and the late-cancellation case returns at
`Engine/Source/Network/Client/ClientReceive.cpp:240-252`. The decompressed frame is first used only when it is
moved into the buffered `ReceivedCoordFullState` at
`Engine/Source/Network/Client/ClientReceive.cpp:254-257`.

Every rejected packet therefore pays a full-frame allocation and LZ4 decompression for nothing. This is wasted
work only — a rejected packet already mutates no slot state, so there is no correctness defect and no
determinism or CRC exposure.

Found as an out-of-scope residual of the session that fixed the load/reset coordinate-channel race; that
change scoped itself to reset and epoch-barrier behavior and did not reorder this function.

## Design

Move the `DecompressAndReadFrame` call and its failure handling below the classification and cancellation
early-return paths, so it runs only once the packet is committed and its frame will actually be buffered.

`ClassifyFullState` takes only `uiSlotIndex`, `uiEpoch`, and `coord`, all read from the fixed message header
that `NetworkMessages::Read` already validated at
`Engine/Source/Network/Client/ClientReceive.cpp:186-194`, so it has no dependency on the decompressed frame
and the reorder is behavior-preserving for every accepted packet.

The existing decompression-failure handling — the `kNetwork`/`kWarning` log and early return at
`Engine/Source/Network/Client/ClientReceive.cpp:207-211` — moves with the call and keeps its current wording,
so a corrupt payload on a committed packet still fails the same way. The observable change is that a payload
which fails to decompress but would have been rejected by classification anyway no longer logs that warning;
that packet was discarded either way.

Keep the existing `ScopedSuppressAllocationTracking` covering the decompression allocation.

## Critical files

- `Engine/Source/Network/Client/ClientReceive.cpp` — `Client::ServerCoordFullState` only; the decompression
  call site and its failure branch move below the classification and cancellation early returns.
- `Engine/Source/Network/Client/ClientReceive.cpp` `ClassifyFullState` — read-only evidence that
  classification consumes only header fields.

## In scope

- The statement order inside `Client::ServerCoordFullState`: relocating the `DecompressAndReadFrame` call, its
  null check, and its warning log to just before the `ReceivedCoordFullState` construction.

## Out of scope

- `ClassifyFullState`, `ClassifyCoordUpdate`, `ServerCoordStaticData`, `ServerCoordUpdateOrResend`,
  `ServerSubscribeAccept`, `ServerUnsubscribeAck`, and every other receive handler.
- `DecompressAndReadFrame` itself, LZ4 usage, frame reader code, and any compression format or bound change.
- Slot, epoch, ACK, ghost-unsubscribe, and cancellation semantics; the unsubscribe sends and their ordering
  relative to each other stay exactly as they are.
- Any packet layout, `PacketType`, protocol version, `Frame::kiVersion`, or server source change.

## Risk tier and invariants

Change Workflow Tier 2 — scoped client transport behavior in one subsystem. The reorder touches no wire
layout, serialization format, save/replay compatibility, threading model, or CRC-covered state; the buffered
`ReceivedCoordFullState` contents are unchanged.

Invariants to preserve: a committed full state still buffers exactly the frame it does today; a rejected or
ghost packet still sends the same epoch-qualified unsubscribe and mutates no slot state; the decompression
allocation stays inside `ScopedSuppressAllocationTracking`.

## Acceptance criteria

- Structural inspection shows no early-return path in `ServerCoordFullState` reaches `DecompressAndReadFrame`,
  and that every committed path still buffers a decompressed frame with the unchanged failure handling.
- Both Debug x64 targets compile.
