# Engine Network - Transport and Protocol Infrastructure

Shared ENet transport, slot subscriptions, ACK state, discovery, wire cursors, and hostile-input enforcement. Game packet payloads and reconciliation policy belong to game Network (`../../../Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`); flow and timing details live in Network architecture (`../../../Documents/Architecture/Network.md`).

## Transport Contracts

- Use `NetworkManager` channel helpers for control and per-coordinate reliable/unreliable channels. All sends go through `SendPacket`; opaque game payloads use `SendSimplePacket`, while engine packets use `NetworkMessages` layouts.
- Each coordinate slot has independent ACK floor, bitfield, and epoch state (epoch is the reuse counter). Epoch mismatch drops stale traffic after slot reuse; unsubscribe carries the observed epoch, so a stale request cannot free a reused slot while the server still ACKs the no-op.
- Engine packet types remain below `kGamePacketStart`; game packets are forwarded opaquely.
- The handshake verifies protocol version, deterministic Frame version, and ordered island-manifest identity before accepting a peer.
- Every incompatible wire change increments `engine::kuiProtocolVersion`: a game `StatusChange` or `TransferData` layout change is bumped by its game codec owner, and an engine packet layout change or an added engine packet type is bumped here, because inserting a type before `kGamePacketStart` shifts every game packet identifier. Engine Network owns the shared version and Hello rejection gate.
- `Frame::kiVersion` is a separate deterministic-Frame/save/replay compatibility gate, not a substitute for the protocol-version bump.
- Cursor primitives are unchecked. Validate exact fixed layouts or gate every variable-length read with `BoundedCursor` before passing its cursor to a primitive.
- Client-to-server packet additions require a contract row, size and semantic validation, handshake/debug gating where applicable, per-tick rate limits, and violation reporting through the server contract path. Send commands at tick cadence, not render cadence; that cadence rule governs per-tick rate-limited command generation, so immediately flushing an already-authorized rare user request is permitted because it changes departure time, not packet count.
- Client and server both disable the ENet peer throttle and set 1 MB socket send and receive buffers. The two sides are deliberately paired; tuning only one leaves the ends disagreeing about how much burst traffic they will absorb.
- ENet service, discovery polling, sends, and simulation queues are main-thread-only. Their workbuffer and state have no locking by design.

## Timing and Polling

- Rendering smoothness takes priority over command round-trip latency. Client simulation and presentation intentionally retain buffered committed ticks; tuning should preserve smooth pacing rather than introduce stalls or bursts.
- Both peers drain transient poll outputs each poll, and a server update polls more than once, so a queue left unconsumed across a poll is lost or reapplied rather than carried to the end of the update. New-subscription and resync requests persist until broadcast servicing, including the paused/zero-tick path.
- Network simulation injects deterministic one-way delay and burst loss above ENet. Reliable packets may be delayed but are never deliberately dropped, and each channel preserves FIFO release order.
- Wire serialization is little-endian x64. Network buffer capacity is tick-rate-independent; jitter safety is wall-clock time.

## Ownership

- `NetworkManager` owns ENet lifetime, channel math, and the allocation-suppressed send path.
- `NetworkProtocol` owns shared packet identifiers, compatibility constants, slot identity, ACK structures, and client-to-server contract rows.
- `NetworkDiscoveryResponder` (server) and `NetworkDiscoveryScanner` (client) own LAN discovery — a second, self-contained wire protocol that does not use ENet: raw non-blocking UDP on `kuiDiscoveryPort` (`kuiDefaultPort + 1`), carrying nothing but the 4-byte magic number `kuiDiscoveryMagic` ("BRKN") in each direction. The two halves must change together: because the reply carries no payload, the client takes the server's address from the reply's sender address and connects on the default game port. The scanner pings loopback before it broadcasts to the LAN so a server on the same machine always wins the race, and `--loopback-only` stays symmetric — it binds the responder to loopback and makes the scanner skip the broadcast.
- `NetworkMessages` owns engine packet field order, sizes, and paired encode/decode; evolve an engine packet through its shared layout rather than mirroring fields in client/server leaves.
- Client and server leaves own the side-specific transport peers, receive buffers, contracts, slots, and packet handling.
- `ClientSessionRuntime` and `ServerSessionRuntime` own reusable connection/discovery, subscription/queue, clock/pacing, poll, flush, resend, and reset sequencing. Canonical game sessions compose them and supply synchronous typed policy hooks.
- `game::NetworkSessionContract` supplies Frame/status types, protocol constants, codecs, and game packet contracts at compile time. Runtimes use no virtual session base, runtime type erasure, or additional global manager.
- `NetworkSerialization` owns the status-change batch codec body: the per-type group envelope, the LZ4 framing, and the bounded receive path. Game Network owns the status-change type set, the payload formats, and the per-type read/write operations the codec calls through `game::NetworkSessionContract`.
- `ServerTransferManager` and `ServerBroadcaster` own cross-cell transfer handling and per-cell publication assembly. Game Network supplies the `StatusChange` payloads they carry and the transient policy queues they drain; Server (`Server/AGENTS.md`) records the deliberate game-type ownership that entails.
- A status-change batch is all-or-nothing at both ends. The server drops a whole over-cap batch rather than send part of one, and the receive side returns zero changes on any malformed byte, an out-of-range uncompressed-size prefix, or a decoded count past the caller's cap. Applying part of a batch would silently desync the cell; whole-batch rejection leaves the frame's CRC to mismatch, which resyncs through the existing path.

## See Also

- Client (`Client/AGENTS.md`) - Client transport peer, receive guards, slot state, and metrics
- Server (`Server/AGENTS.md`) - Server transport host, contracts, slots, and resend state
- Game Network (`../../../Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`) - Concrete session policy and game wire payloads
- Network architecture (`../../../Documents/Architecture/Network.md`) - Protocol flow and ACK/reconciliation timing
- Game reconciliation (`../../../Documents/Architecture/GameReconciliation.md`) - Rollback and replay integration
