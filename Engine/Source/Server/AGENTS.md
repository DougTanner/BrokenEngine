# Server Monitor Host

Server-only statistics aggregation and the separate Server Monitor channel.

## Ownership and Cadence

- `Main.cpp` owns the host after the game and NetworkManager, and stops and joins
  it before those objects or the profile and file managers are destroyed.
- The main thread aggregates statistics on every update with the existing
  profiling configuration gates, preserving raw `query_profile` rows even
  without a monitor. Every eighth update while authenticated, it captures
  owning text strings and cell records. Capture reads game, ENet and profile
  state only on the main thread, using the profile formatters' existing locks.
- One mutex-protected latest immutable snapshot slot replaces unsent state.
  The mutex covers pointer exchange only; allocation, formatting, serialization,
  socket operations and replaced-snapshot destruction stay outside it.
- An ordinary `ThreadLocal::Entry` jthread owns sockets, authentication and wire
  serialization. It reads only host-owned state and immutable snapshots, never
  live manager or game objects. Startup waits for listen success or its logged
  exception. Stop abandons pending frames and closes sockets on that worker;
  joining holds no snapshot mutex and needs no transport flush.
- Main-thread capture and the worker lifetime have separate allocation-tracking
  suppression scopes.
- Deliberate game-type ownership: this directory consumes
  `game::GetServerCellStatistics`, `game::ServerCellStats`, and
  `game::kServerEntityCountNames`, the labels of the per-cell population rows
  (the game project's own `Source/Frame/AGENTS.md`).

## Monitor Channel

The channel is non-ENet IPv4 TCP on port 27017, with one accepted connection
and backlog 1. The main thread reads `ServerMonitorSecret.txt` from the server
app-data directory and copies its trimmed key into host-owned storage. An empty
key or `--loopback-only` binds 127.0.0.1; otherwise it binds all interfaces.
No socket reuse option permits another process to take the port.

`Common/ServerMonitorProtocol.h` owns the little-endian x64 layout, magic,
version, bounds, key trimming and HMAC helper. A frame header carries body size
and type. The challenge contains magic, version and a fresh 32-byte random
nonce. The monitor sends exactly 32 raw HMAC-SHA256 response bytes using the
shared secret as key; the secret never travels. With no key any 32 bytes are
accepted, because the listener is loopback-only. One response attempt is allowed
per connection. The server never reads a client-controlled allocation size.

Receive requests cover only the missing response bytes. Mismatch, five seconds
from accept without a complete response, or any byte after authentication closes
the connection and logs rejection. EOF and socket errors other than
`WSAEWOULDBLOCK` disconnect. Would-block preserves progress. Unexpected inbound
data is polled even during a partial outbound frame. Each challenge or snapshot
must send within three seconds of its first send attempt; a frame completes
before another starts. Every wait is at most 50 ms and observes stop.

Snapshot bodies carry statistics sections, profile sections, then cell records.
Text sections join labels to values and include their display color. Cells are
the union of active, client-authorized and actively subscribed coordinates;
counts distinguish authorizations and active entities, and flags distinguish
active and subscribed cells. The server asserts the maximum frame bound.
Server-to-monitor data is trusted; the monitor checks protocol magic/version.

## Profile Presentation

Use one `TickVisibilityCadence` decision per snapshot for both CPU timer and
counter formatters. Build transient text in the thread-local workbuffer and
copy it into owning snapshot strings. Display rows hide sustained zeros;
raw server agent profile queries include them. Profiling-disabled builds keep
the same configuration policy and never read absent raw-timer storage.
