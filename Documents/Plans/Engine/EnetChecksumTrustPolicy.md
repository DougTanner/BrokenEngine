<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T22:51:35.624Z","dependsOn":[]} -->
# Enable the ENet datagram checksum and record the direction-based trust-boundary policy

## Context

User decisions this session, recorded here as decided:

1. "All incoming network records (client and server) must be checked for
   non-corruption."
2. Client to server: the server makes certain that every incoming record is
   uncorrupted, with values in their expected range and never NaN or infinite.
   An out-of-range value is rejected, never clamped (Q8a).
3. Server to client: ENet's built-in per-packet checksum protects the traffic,
   and it is enabled on both hosts (Q7a). The client trusts server data fully.
   It drops both value checks and structural or memory-safety checks on server
   data (Q6b).
4. Replays are exempt because they are debug-only today. This is a narrow
   exception to always checking at boundaries. The agent-harness command
   channel is exempt in the same way, as a developer tool (Q9a).
5. Saves are server input and get full validation (Q10).
6. "All of this should be recorded in AGENTS.md files at the relevant level."

Line numbers are at `17c1ca52`; re-derive them at claim.

Neither host enables the checksum today. `enet_host_create` leaves
`host->checksum` NULL (`ThirdParty/enet/host.c:89`). The client creates its
host at `Engine/Source/Network/Client/Client.cpp:110`, and the server at
`Engine/Source/Network/Server/Server.cpp:34`. ENet supplies the field
(`ThirdParty/enet/include/enet/enet.h:337`, `:381`) and the stock callback
`enet_crc32` (`enet.h:564`). When the field is set, the sender writes a 4-byte
checksum into every datagram header (`ThirdParty/enet/protocol.c:1709-1717`),
and the receiver silently drops any datagram whose checksum mismatches
(`protocol.c:1075-1089`). The checksum covers both directions and every
datagram, including the Hello exchange. If only one peer enables it, every
datagram is dropped, so the Hello protocol-version gate never gets to report
the mismatch.

The current policy text conflicts with decisions 1-5 in several places:

- `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy` makes semantic
  value checks part of corruption for both directions.
- `Common/AGENTS.md` `## Shared Data Contracts` treats replay and network
  counts as untrusted in both directions.
- `.agents/references/cpp-conventions.md` says to validate all network input,
  with no direction.
- `Engine/Source/File/AGENTS.md` `## Replay Streams` and
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` call the replay stream a
  trust boundary.
- The root `AGENTS.md` has no trust-boundary statement.

The survey placed ownership as follows. Direction rules and the checksum go to
Engine Network, the save rule to File `## Grid Saves`, and the replay exemption
to File `## Replay Streams`.

## Design

Author's recommendations, with rationale:

- **Checksum.** Set `mpHost->checksum = enet_crc32;` right after each successful
  `enet_host_create` in the `Client` and `Server` constructors. The stock
  callback is the smallest change that meets decision 3. It needs no change to
  the engine message format, and the ENet receive path already drops a
  mismatched datagram before it reaches the engine.
- **Protocol version.** Increment `engine::kuiProtocolVersion` by one from its
  value at claim (`Engine/Source/Network/NetworkProtocol.h:63`, `16` at
  `17c1ca52`). Update the current-version sentence in
  `Documents/Architecture/Network.md` `## Client → Server Contract` to match.
  The datagram header gains 4 bytes. `Engine/Source/Network/AGENTS.md`
  `## Transport Contracts` requires a bump for every incompatible wire change.
  A mismatched build still fails at connect as a timeout rather than a Hello
  rejection, and the new Engine Network bullet records that.
- **Policy text.** Write each fact once, at its owner:
  - Root `AGENTS.md` `## Key Patterns`: one bullet. It states that the server
    fully validates every client-to-server record (reject, never clamp) and
    every grid save; that server-to-client data travels under ENet's datagram
    checksum and the client trusts it without checks; and that replays and the
    agent command channel are exempt developer tools. It routes to the two
    owners below.
  - `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`: rewrite by
    direction. The server rejects any client record whose layout, size, count,
    or value is invalid (non-finite, outside its expected range, or a broken
    relationship), and never clamps or substitutes. The client adds no value or
    structural checks on server data. Keep the existing sentences on the
    protocol-state exclusions, the throw-at-detection signal, and the
    asymmetric dispatch answers: shared readers still throw, and the client
    catches still assert.
  - `Engine/Source/Network/AGENTS.md` `## Transport Contracts`: add the
    checksum to the paired peer-configuration bullet. Both hosts set
    `enet_crc32`, and enabling it on one side alone drops every datagram. Scope
    the cursor bullet's validation rule to readers of client data.
  - `Documents/Architecture/Network.md`: correct the violation-policy
    sentence that says the threshold tolerates in-transit UDP corruption. ENet
    now drops such a datagram before dispatch.
  - `Common/AGENTS.md` `## Shared Data Contracts`, the `Serialization.h`
    bullet: keep the count/capacity helper rule and the `.pack` rule. Replace
    the per-source list (saves, replays, network) with a pointer to the owners
    above.
  - `.agents/references/cpp-conventions.md`, the error-handling bullet: replace
    the undirected "network input" with a pointer to the Engine Network
    `## Corrupt Input Policy`.
  - `Engine/Source/Frame/Collections/AGENTS.md`, the deserialization bullet:
    state that `PostRead` validation exists for the server's save read, and
    that it also runs on client network reads, where it is redundant but
    harmless.
  - `Engine/Source/File/AGENTS.md` `## Replay Streams`: add a bullet. Replay
    streams are exempt from trust-boundary validation because replays are
    debug-only. Existing replay checks may stay, and no new ones are required.
  - `Engine/Source/Agent/AGENTS.md` `## Architecture`: extend the "Command
    handlers throw for invalid external input" bullet. The command channel is
    an exempt developer tool, and handlers need not range-check arguments.
  - `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, the variant-read
    bullet: drop the "trust boundary" framing for the replay reader. Keep the
    count bound and unknown-tag rejection as read-correctness checks, because a
    bad tag seats the wrong variant.
  - `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp`: reword the two
    `// Trust boundary (replay stream)` comments in `operator>>` to match. The
    code does not change.

## Critical files

- `Engine/Source/Network/Client/Client.cpp` and `Engine/Source/Network/Server/Server.cpp` — host construction.
- `Engine/Source/Network/NetworkProtocol.h` — `kuiProtocolVersion`.
- `ThirdParty/enet/include/enet/enet.h`, `ThirdParty/enet/protocol.c` — checksum API (read only).
- `AGENTS.md`, `Engine/Source/Network/AGENTS.md`, `Documents/Architecture/Network.md`, `Common/AGENTS.md`, `.agents/references/cpp-conventions.md`, `Engine/Source/Frame/Collections/AGENTS.md`, `Engine/Source/File/AGENTS.md`, `Engine/Source/Agent/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp`.

## In scope

- One `checksum = enet_crc32` assignment each in the `Client` and `Server`
  constructors.
- The `kuiProtocolVersion` increment and the `Network.md` current-version
  sentence.
- The documentation and comment edits listed under `## Design`, and no others.

## Out of scope

- Removing any client-side check on server data. A later Plan that depends on
  this one owns that, together with the documentation lines that describe those
  specific checks: Engine Network's batch-codec receive bullet, Engine Network
  Client `## Subscription Receive Invariants`, Engine Frame's navigation
  deserialization bullet, the game Frame spawn-timer bullet, and the client
  catch comments.
- Server-side validation changes. A prerequisite Plan closes those before this
  one.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/AGENTS.md`.
  The change that retired the transfer fire-time checks owns its wording.
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`, whose
  "save/replay fleet data" wording describes one reader that both paths use and
  stays true.
- `ServerFleetSerialization.cpp` comments, which describe the save boundary
  accurately.
- A custom checksum, per-record or per-batch checksums, engine message trailers,
  a new ENet version, and LAN discovery (raw UDP outside ENet).
- `Frame::kiVersion`, `FrameInput::kiVersion`, and the save format.

## Risk tier and invariants

Change Workflow Tier 3. Triggers: a wire/protocol change (the ENet datagram
header gains a checksum, and the protocol version bumps) and a trust-boundary
change (server-to-client integrity now rests on the transport checksum).

Invariants:

- Both hosts enable the same checksum callback. A one-sided build cannot
  exchange any datagram.
- No engine or game message layout changes. Deterministic Frame state and CRC
  are unaffected.
- Each policy fact appears once, at its owner. The root bullet only states the
  constraint and routes.
- Every documentation statement is true at landing. Statements about client
  checks describe the rule, not the removal of checks that still exist.

## Acceptance criteria

- The diff shows the two assignments, the protocol increment, and the
  documentation edits listed above, with no other source change.
- A live client connects to a live server built from the same tree, subscribes,
  receives full states and deltas, and runs for at least a minute with no
  desync or disconnect, both with and without the network simulation's delay.
- Client and Server `Debug|x64` build clean through `/compile`.
- `/progressive-disclosure-review` reports no duplicated policy fact across the
  edited AGENTS.md files.

## Notes

ENet's checksum covers transit corruption only. Decision 3 accepts that the
client does not guard against a server bug.
