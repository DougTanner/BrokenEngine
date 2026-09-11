# Engine Network - Transport and Protocol Infrastructure

Shared ENet transport, slot subscriptions, ACK state, discovery, wire cursors, and hostile-input enforcement. Game packet payloads and reconciliation policy belong to game Network, and flow and timing detail to Network architecture; both are linked under `## See Also`.

## Transport Contracts

- Use `NetworkManager` channel helpers for control and per-coordinate reliable/unreliable channels. All sends go through `SendPacket`; opaque game payloads use `SendSimplePacket`, engine packets use `NetworkMessages` layouts.
- Each coordinate slot has independent ACK floor, bitfield, and epoch state (epoch is the reuse counter). Epoch mismatch drops stale traffic after slot reuse; unsubscribe carries the observed epoch, so a stale request cannot free a reused slot while the server still ACKs the no-op.
- Load generation identifies the server's current debugging-load world across connection bootstrap and load-scoped subscription traffic. Both receivers compare it with their own before applying the slot epoch or mutating transport/game state; a mismatch logs a Network warning and returns. Epoch stays the reuse counter within one load generation. Wire and reset behavior: [Network architecture](../../../Documents/Architecture/Network.md#debug-load-generation).
- Engine packet types remain below `kGamePacketStart`; game packets are forwarded opaquely.
- Every incompatible wire change increments `engine::kuiProtocolVersion`: a game `StatusChange` or `TransferData` layout change is bumped by its game codec owner; an engine packet layout change or an added engine packet type is bumped here, because inserting a type before `kGamePacketStart` shifts every game packet identifier. Engine Network owns the shared version and the Hello rejection gate the handshake applies (`Server/AGENTS.md`). `Frame::kiVersion` is a separate deterministic-Frame/save/replay compatibility gate, not a substitute for the protocol-version bump.
- Cursor primitives are unchecked. Validate exact fixed layouts or gate every variable-length read with `BoundedCursor` before passing its cursor to a primitive.
- A client-to-server packet is admitted only through its contract row, which owns size bounds, per-tick caps, handshake/debug gating, and violation reporting ([Client to Server Contract](../../../Documents/Architecture/Network.md#client--server-contract)). A new packet adds a row there; its handler keeps only the residual semantic validation the row names. Send commands at tick cadence, not render cadence; immediately flushing an already-authorized rare user request is permitted because it changes departure time, not packet count.
- Client and server both disable the ENet peer throttle and set 1 MB socket send and receive buffers. The pairing is deliberate; tuning one side alone leaves the ends disagreeing about how much burst traffic they absorb.
- ENet service, discovery polling, sends, and simulation queues are main-thread-only. Their workbuffer and state have no locking by design.

## Corrupt Input Policy

- Corrupt network data is a failure of the declared wire layout or of a bounded payload codec, or an invalid semantic value or relationship that must hold before the payload is adopted, such as a non-finite vertex or an invalid topology or index relationship. A structurally valid packet rejected for protocol state or timing — stale or reordered traffic, epoch or slot reuse, subscription state, pre-handshake arrival, a rate limit, a protocol/version mismatch, or an ordinary resync — is not corruption and keeps its own outcome.
- Readers signal corruption one way: they throw `std::ios_base::failure` where they detect it rather than returning a sentinel, and the policy lives at the dispatch catches — the one exception being the server's ack-stream cross-check, which records its own violation locally.
- The two directions answer that signal asymmetrically. Corrupt data from its own server is fatal to the client: the dispatch `ASSERT`s, so the process ends through crash reporting. Corrupt data from a client is hostile but survivable to the server: its catches drop the packet before any mutation and count one violation through `RecordContractViolation` (`Server/AGENTS.md`), so no client can end the host.

## Timing and Polling

- Rendering smoothness takes priority over command round-trip latency: client simulation and presentation intentionally retain buffered committed ticks, so tuning must preserve smooth pacing rather than introduce stalls or bursts.
- Both peers drain transient poll outputs each poll, and a server update polls more than once, so a queue left unconsumed across a poll is lost or reapplied rather than carried to the end of the update. New-subscription and resync requests persist until broadcast servicing, including the paused/zero-tick path.
- Network simulation injects deterministic one-way delay and burst loss above ENet. Reliable packets may be delayed but never deliberately dropped, and each channel preserves FIFO release order.
- Wire serialization is little-endian x64. Network buffer capacity is tick-rate-independent; jitter safety is wall-clock time.

## Ownership

- `NetworkManager` owns ENet lifetime, channel math, and the allocation-suppressed send path.
- `NetworkProtocol` owns shared packet identifiers, compatibility constants, slot identity, ACK structures, and client-to-server contract rows.
- `NetworkDiscoveryResponder` (server) and `NetworkDiscoveryScanner` (client) own LAN discovery — a second, self-contained wire protocol that does not use ENet: raw non-blocking UDP on `kuiDiscoveryPort` (`kuiDefaultPort + 1`), carrying nothing but the 4-byte magic number `kuiDiscoveryMagic` ("BRKN") each way. The two halves must change together: the reply carries no payload, so the client takes the server's address from the reply's sender and connects on the default game port. The scanner pings loopback before broadcasting to the LAN so a same-machine server always wins the race, and `--loopback-only` stays symmetric — responder bound to loopback, scanner skipping the broadcast.
- `NetworkMessages` owns engine packet field order, sizes, and paired encode/decode; evolve an engine packet through its shared layout rather than mirroring fields in client/server leaves.
- Client and server leaves own the side-specific transport peers, receive buffers, contracts, slots, and packet handling.
- `ClientSessionRuntime` and `ServerSessionRuntime` own the reusable session mechanics and phase order that canonical game sessions compose with synchronous typed policy hooks ([Network architecture](../../../Documents/Architecture/Network.md#session-ownership-and-phase-order)). `game::NetworkSessionContract` supplies Frame/status types, protocol constants, codecs, and game packet contracts at compile time; runtimes use no virtual session base, runtime type erasure, or additional global manager.
- `NetworkSerialization` owns the status-change batch codec body: the per-type group envelope, the LZ4 framing, and the bounded receive path. Game Network owns the status-change type set, the payload formats, and the per-type read/write operations the codec calls through `game::NetworkSessionContract`. Batches emit non-empty type groups in ascending `StatusChangeType` order, preserving input order within each type; published server tick inputs use that sequence because clients apply the wire order, so changing it can alter index-sensitive state and the CRC.
- `ServerTransferManager` and `ServerBroadcaster` own cross-cell transfer handling and per-cell publication assembly, including the deliberate game-type ownership that entails (`Server/AGENTS.md`).
- A status-change batch is all-or-nothing at both ends: the server drops a whole over-cap batch rather than send part of one, and the receive side throws on any malformed byte, an out-of-range uncompressed-size prefix, or a decoded count past the caller's cap, rejecting the whole batch under the corrupt-input policy above. Applying part of a batch would silently desync the cell.

## See Also

- Client (`Client/AGENTS.md`)
- Server (`Server/AGENTS.md`)
- Game Network (`../../../Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`)
- Network architecture (`../../../Documents/Architecture/Network.md`)
- Game reconciliation (`../../../Documents/Architecture/GameReconciliation.md`)
