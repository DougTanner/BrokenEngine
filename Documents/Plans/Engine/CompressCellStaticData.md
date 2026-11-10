<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-10T23:38:14.430Z","dependsOn":[]} -->
# Compress and reuse server cell static payloads

## Context

The user requested selective compression where packet size and bandwidth savings justify it. They explicitly
requested compressing reusable data once per unchanged version and keeping elevation grids local. This plan
recommends cell static data as the first candidate. The server currently serializes it for every subscription
send. Its body contains island placements and authoritative navigation data. The client already builds elevation
from placements and packed heightmaps. See `Engine/Source/Network/Server/ServerBufferedFrames.cpp:205`,
`Engine/Source/Frame/CellStaticData.cpp:8`, `Engine/Source/Network/Client/Client.cpp:479` and
`Engine/Source/Frame/FrameBase.cpp:299`.

## Packet-family assessment

- Cell static data is currently raw, with navigation graphs varying by coord. Measure representative payload sizes including transport framing, and implement cached static compression if the weighted subscription mix shows material savings. Include island-rich and small/empty cells.
- Live full states are LZ4-compressed for each send. Initial subscriptions, explicit resync and recovered
  streams send the current authoritative frame. Inspect whether repeated requests send identical committed
  snapshots often enough to justify memoization. Reusing compressed full states would reduce CPU work and would
  send the same bytes. Retain the current format and report independently measured reuse opportunities. See
  `Engine/Source/Network/Server/ServerBufferedFrames.cpp:160`,
  `Engine/Source/Network/Server/ServerBufferedFrames.cpp:284` and
  `Engine/Source/Network/Server/ServerSessionRuntime.cpp:190`.
- Delta and resend status bodies already serialize and compress once per coordinate/tick and reuse the retained
  bytes. Preserve that mechanism and readable transport metadata. Debug full frames already serialize once per
  retained coordinate/tick but compress on request. Report repeated identical diagnostic requests if observed
  and leave the current Debug-only behavior in place for this static-data change. See
  `Engine/Source/Network/Server/ServerBufferedFrames.cpp:65`,
  `Engine/Source/Network/Server/ServerBufferedFrames.cpp:77`,
  `Engine/Source/Network/Server/ServerBufferedFrames.cpp:232` and
  `Engine/Source/Network/Server/ServerBufferedFrames.cpp:558`.
- Connection, subscription, pause/timespeed, load and owned-entity control, chat and BES fleet sync are raw.
  Assess size and frequency in representative traffic. Preserve their existing layouts in this static-data
  change. Record any measured material opportunities for follow-up with the exact family, bytes, frequency and
  encode/decode cost. See `Engine/Source/Network/Server/Server.cpp:608`,
  `Engine/Source/Network/Server/Server.cpp:809`, `Engine/Source/Network/Server/Server.cpp:899`,
  `Engine/Source/Network/Server/ServerSessionBase.cpp:280` and
  `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:25`.
- LAN discovery sends a fixed UDP magic response. Server Monitor has a separate versioned authentication and TCP snapshot protocol. Preserve both in this gameplay static-data change. See `Engine/Source/Network/NetworkDiscoveryResponder.cpp:51`, `Engine/Source/Server/ServerMonitorHost.cpp:342` and `Tools/ServerMonitor/ServerMonitor.cpp:96`.

## Design

The design below is the plan author's recommendation for Tier-3 review and user approval after claim.

Use the existing LZ4 block codec. Reuse the full-state packet's explicit uncompressed-length convention and keep
readable load generation, slot, epoch and coordinate fields. Use one versioned static layout with the existing
encoded payload length and a signed 32-bit uncompressed length. Zero indicates the raw body, since the static
writer always emits both counts. A positive value identifies an LZ4 body and declares its decoded length. Choose
LZ4 when it produces a smaller packet than the raw encoding of the same body. Quantify the complete framing
difference against the original packet across the mixed workload. Both encodings yield the same exact static
bytes.

A server-only cache lives with the authoritative Cell and is populated lazily on the first static send after
navigation is built. Retain the chosen encoded bytes and required length metadata together. Discard the cache
with the cell instance on retirement, replacement and load. An in-place change to serialized placements or
navigation starts a new static payload version and invalidates the cache before its next send. Confirm the
placement and nav writers in `Engine/Source/GameBase.cpp:1059`, `Engine/Source/File/GridSave.cpp:116`,
`Engine/Source/Network/Server/ServerSessionRuntime.cpp:131`,
`Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp:23` and
`Engine/Source/Frame/FrameBase.cpp:280`. Invalidate at any writer that can replace already built static data
without destroying the owning Cell. The cache stays out of frame serialization, replay, save and CRC. Frame
publication and per-recipient packet metadata stay separate.

Keep the packet copy and callback so an outstanding ENet packet is independent of cache lifetime. Preserve the
existing gate that serves one pending subscription entry at a time per client. An erased or restarted entry's
static packet can still be in flight. The current-tick initial full state waits for ENet delivery confirmation
of its entry's static packet. See `Engine/Source/Network/NetworkManager.h:46` and
`Engine/Source/Network/Server/ServerSessionRuntime.cpp:224`.
Client checks generation, slot and epoch before decoding the static block, then requires a successful codec
result of the declared size before invoking the existing trusted static reader. Preserve placement and nav bytes
exactly, including their ordering. Bound sizes at their codec and packet representations. Preserve the
authority/trust model in Engine/Source/Network/AGENTS.md.

Increment the gameplay protocol version in `Engine/Source/Network/NetworkProtocol.h:83`. Keep the existing Hello and rejection layout so old/new peers reject at the existing version check (`Engine/Source/Network/Server/Server.cpp:619`). The client accepts the current static packet layout only. Frame/save/replay formats remain as before.

## Critical files

- Engine/Source/Network/Server/ServerBufferedFrames.cpp and Engine/Source/Network/Server/ServerBufferedFrames.h own serialization, compression scratch and static sends.
- Engine/Source/Network/Server/ServerSessionRuntime.cpp and Engine/Source/GameBase.h own authoritative Cell lifetime and subscription ordering.
- Engine/Source/Network/NetworkMessages.h, Engine/Source/Network/NetworkProtocol.h and Engine/Source/Network/Client/Client.cpp own the static layout, version gate and decode.
- Engine/Source/Frame/CellStaticData.cpp owns the exact static body.

## In scope

- ServerBufferedFrames::SendCoordinateStaticData and its encode/cache path, server Cell cache ownership and invalidation, ServerCoordStaticDataMessage, Client::ServerCoordinateStaticData, kiProtocolVersion and relevant network documentation.
- Traffic and CPU measurement for static compression and current outbound family inventory. Record independently observed opportunities and their evidence.

## Out of scope

- Changes to subscription scheduling, other packet formats or cache policies, inbound packets, control headers, separate LAN discovery and monitor transports, and broad transport compression.
- Elevation-grid transmission, frame/save/replay layouts, placement and navigation algorithms, old static wire compatibility, and unit tests.

## Risk and invariants

Tier 3. The static wire format changes across the client/server boundary. Static delivery precedes initial full state, subscription slot and load ordering stay intact, and caching must never cross the lifetime of the exact input. Elevation remains local. The LZ4 result size is checked before deserialization.

## Verification

- Before the first edit, capture representative mixed static sends on identical packed data and coords with at
  least two clients, plus timings and aggregate traffic by packet family. Sample island-rich/nav-heavy and
  small/empty cells, including multi-client join and repeat subscription. Record raw body/packet bytes,
  serialization and compression counts, encode/decode time, retained cache bytes and static transfer cost.
  Compare complete packet bytes after the change, including added framing on raw static bodies. Proceed only if
  the representative mix shows substantial savings against measurement variation and the added encode/decode
  cost is justified. Revert the compression optimization if its acceptance measurement fails. Collect Profile
  x64 timing separately from Debug correctness.
- Assert by trace that each unchanged static payload version encodes once for repeated recipients and that
  replacement, retirement/recreation, replay restore and save/load require a fresh encode for the next send.
  Where an in-place writer exists, exercise that invalidation as well. Compare the decoded payload exactly to
  the reference placement/nav bytes. Verify compressed and raw current-format cases, and inspect the
  decompressed stream for absence of an elevation grid. Remove temporary diagnostics before landing.
- In Debug test simultaneous subscriptions, fragmented or delayed static data, paused join, unsubscribe and slot reuse, resync, save/load, reconnect and mixed-version rejection. Verify one pending entry served at a time per client and static-before-full delivery for each entry, including correct ACK/activation and no stale adoption after a restart or erase.
- Complete a deterministic replay loop across a cell transition with the existing checksum and desync signals. Use the compile and agent-harness skills for BES client and server Profile x64 and Debug x64.

## Execution card

### What does this plan do?

It measures the candidate static-data bandwidth reduction and caches the chosen encoding per authoritative Cell. It sends the cached static body on demand and preserves the client-local elevation grid.

### Why this is good for the codebase

Static subscriptions currently serialize the same placement and navigation bytes for each recipient. Reusing the result removes repeated preparation, and worthwhile compression reduces the static transfer size.

- Goal: Reduce static-data bandwidth and serialize and encode each reusable static version once.
- Out of scope: Changes to other packet formats or cache policies, elevation transmission and save/replay changes.
- Tier trigger: Tier 3, changed client/server static wire layout (`Engine/Source/Network/NetworkMessages.h:840`).
- Interfaces and invariants: Current gameplay protocol version only, clean mixed-version rejection, exact static bytes, cache ownership by current Cell lifetime and existing reliable delivery order.
- Acceptance checks: Profile traffic/cost baseline and Debug live/replay baseline before the first edit. Material aggregate packet-byte reduction or revert, exact decode, once-per-unchanged-version encode, clean lifecycle and replay checks.
- Roles: Plan alternatives, simplicity review, plan audit, grill, implementation and propagation, C++ correctness/style/comment/over-engineering reviews, adversarial review, compile and agent harness, and acceptance verification.

