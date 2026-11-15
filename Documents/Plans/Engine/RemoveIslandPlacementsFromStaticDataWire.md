<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T14:19:47.916Z","dependsOn":[]} -->
# Stop sending island placements and require client string terminators

## Context

This Plan carries two wire changes that share one `kiProtocolVersion`
increment: the static-data placement removal, and the client string
terminator check folded in from the former `RequireClientHelloStringTerminator`
Plan so it rides on this bump instead of its own Tier-3 cycle.

### Island placements

`CellStaticData::Write` (`Engine/Source/Frame/CellStaticData.cpp:44-50`)
starts the `kServerCoordinateStaticData` body with an `int32_t` placement
count and, per placement, `islandCrc`, `f2WorldPosition`, and `fRotation`.
`CellStaticData::Read` (lines 92-100) resizes `islands` to that count and
reads them back.

No receiver uses them:

- `Write` has one caller, `Engine/Source/Network/Server/ServerBufferedFrames.cpp`
  (line 211). `Read` has one caller, `Engine/Source/Network/Client/Client.cpp`
  (line 535), which pushes the record to `mReceivedStaticData`.
- The only drain is `ClientSessionBase` into
  `ClientSessionRuntime::StageStaticData`
  (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:153-163`), which
  moves out `navigationData` alone.
- Client placements come from `GenerateIslandChain`
  (`ClientSessionRuntime.cpp:126`), as
  `Engine/Source/Network/Client/AGENTS.md` `## Live Terrain Preparation`
  states: "Received static data contributes only its navigation."
- Saves regenerate placements (`Engine/Source/File/GridSave.cpp`), and the
  `cell_coordinate_probe` command
  (`Engine/Source/Agent/Commands/CellCoordinateProbe.cpp`) hashes generated
  and live-cell placements, never received ones.

So every static-data packet carries a block the client allocates for
(`islands.resize(iCount)`) and then discards. The unmet condition: the
static-data body carries only what a receiver consumes.

Origin: an architecture review of `Engine/Source/Frame` (architecture phase,
client/server and data-shape lens), validated against commit `ae681f03`.

### Client string terminator

Paths in this subsection are under `Engine/Source/Network/` unless they start
elsewhere. Verified against commit `10d84ca6`; the lines cited are those at
commit `c675b6b2`.

`MessageReader::NullTerminatedString` (`NetworkMessages.h:241-258`) walks to
the NUL or to the packet end and leaves `mbValid` set when it finds no NUL.
The paired writer always appends the NUL (`NetworkMessages.h:123-127`), and
`NetworkMessages::Read` checks only `mbValid` (`:448-457`).

So the server accepts a `kClientHello` no writer can produce:

- `ClientHelloMessage::kiMinSize` equals `kiFixedSize`, 21 bytes
  (`NetworkMessages.h:555-556`). The contract row uses it
  (`NetworkProtocol.h:204`), and the `kClientHello` row of
  `Documents/Architecture/Network.md` says Min 21. A written Hello is at least
  22 bytes.
- `Server::Receive` admits the size at its contract check
  (`Server/Server.cpp:265`), and `Server::ClientHello`
  (`Server/Server.cpp:613-706`) adds no terminator check.
- `kClientChat` reads its text through the same call
  (`NetworkMessages.h:752`), so an unterminated chat string is accepted too.

Observed effect: permissive, not harmful. The scan is bounded by `pEnd`
(`NetworkMessages.h:249`) and the server copy by `std::min`
(`Server/Server.cpp:653`). An unterminated Hello reads as the same Hello
terminated and carrying no GUID, which the layout allows; the server then
mints a GUID (`Server/Server.cpp:691-697`). `OptionalGuid` returns without
consuming when fewer than 16 bytes remain (`NetworkMessages.h:268-271`), so
1 to 15 trailing Hello bytes are never read. An unterminated chat reads as the
same text terminated, and an overlong text fails `IsValidChatText`
(`ChatText.cpp:10`).

Two sources disagree about the intended behavior, and neither says whether
the terminator is required:

- `AGENTS.md` `## Corrupt Input Policy`: a client-to-server record whose
  layout is invalid is corrupt and the server rejects it.
- The Hello contract row, in code and in `Network.md`: minimum 21 bytes, a
  size only an unterminated Hello has.

The user chose in the authoring session that the server reject a string with
no terminator, so the policy holds and the contract row changes to match it.

Originating gap: the architecture phase (client/server and trust-boundary
lens) of an analysis of `Engine/Source/Network` at that commit. Unmet
criterion: "a `kClientHello` or `kClientChat` whose string has no terminator
is rejected as corrupt, or the contract states that the terminator is
optional".

## Design

### Island placements

Recommended: remove the placement block from both functions, so the body
starts at the navigation graph count, and bump `engine::kiProtocolVersion`
(`Engine/Source/Network/NetworkProtocol.h`), because the layout of an engine
packet changes (`Engine/Source/Network/AGENTS.md` `## Transport Contracts`).

Removal is recommended over making the client compare received placements
with generated ones: that would add a client check on server data, which
`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy` rules out, and it
would be a new feature rather than debt removal.

Documentation the change makes wrong, to fix in the same change:

- `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation`: the sentence
  "Static-data deserialization trusts the count under the corrupt-input
  policy" (about `kiMaximumIslandsPerCell`) and the word "placement" in
  "Preserve placement and vertex bits" both describe the removed block.
- `Documents/Architecture/Network.md`: add the new version to the
  version-history paragraph that follows the client-to-server contract table.
- The comment above `PlacementsCrc` in `CellCoordinateProbe.cpp` says the fold
  matches `CellStaticData::Write`'s member order; after the change `Write`
  has no placement members.

### Client string terminator

Paths here follow the `### Client string terminator` subsection of
`## Context`. Require the terminator, in the server build only:

- `MessageReader::NullTerminatedString` clears `mbValid` when it reaches the
  packet end without a NUL (`NetworkMessages.h:254-257`), in a branch guarded
  by `#ifdef BT_SERVER`. `NetworkMessages::Read` then throws, as it does for
  every other invalid layout, and the server's dispatch catch drops the
  packet and counts the violation.
- `ClientHelloMessage::kiMinSize` becomes `kiFixedSize + 1`
  (`NetworkMessages.h:556`).
- The `kClientHello` row of `Documents/Architecture/Network.md` changes Min 21
  to 22.

Rationale: root `AGENTS.md` `## Key Patterns` says the server fully validates
every client-to-server record, and the policy section is the more general
rule. Recording the terminator as optional in `Network.md` was rejected for
that reason.

This part needs a protocol bump too, and the one increment above covers it:
`Engine/Source/Network/AGENTS.md` `## Transport Contracts` bumps "a change to
what an engine packet's contract row requires of its payload content", and
this change raises the `kClientHello` row's minimum size and makes the
terminator a payload requirement of the Hello and chat rows. The bullet's
stated reason, that a client of another build would be rejected, does not
occur here, because no build writes a 21-byte Hello; the bullet's rule is
followed as written. The version-history entry in `Network.md` names both
changes.

Placement, decided: the one guarded branch in the reader. The server build
reads only client strings through this call, the `kClientHello` build
configuration (`NetworkMessages.h:574`, `Server/Server.cpp:615-616`) and the
`kClientChat` text (`NetworkMessages.h:752`, `Server/Server.cpp:577-578`), so
one branch covers both and the check never runs on data the server wrote.
The client build reads `kServerChat` through the same call
(`NetworkMessages.h:768`, `Client/Client.cpp:854-855`); the guard compiles
the check out there, so the client accepts exactly what it accepts today.
That follows `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`:
client code adds no new structural checks on server data. Every `Visit`, and
so each paired layout, stays shared and unguarded.

A check in each of the two server handlers instead was rejected: it is two
sites repeating one comparison of the string's end against the packet end,
and it puts a layout check in handlers that hold only semantic validation.

## Critical files

- `Engine/Source/Frame/CellStaticData.cpp`
- `Engine/Source/Network/NetworkMessages.h`
- `Engine/Source/Network/NetworkProtocol.h`
- `Engine/Source/Frame/AGENTS.md`
- `Documents/Architecture/Network.md`
- `Engine/Source/Agent/Commands/CellCoordinateProbe.cpp` (comment only)

## In scope

- In `CellStaticData::Write`: the placement count and the placement loop.
- In `CellStaticData::Read`: the count read, `islands.resize`, and the
  placement loop.
- `kiProtocolVersion`: one increment of the then-current literal, shared by
  both parts.
- The documentation and comment sites listed in `## Design`.
- `MessageReader::NullTerminatedString`: the no-NUL exit only, as one
  `BT_SERVER`-guarded branch.
- `ClientHelloMessage::kiMinSize`.
- The `kClientHello` row and the version-history paragraph of `Network.md`.

## Out of scope

- The navigation part of `Write`/`Read`: graph count, vertices, gap-encoded
  polygon offsets, visibility edges, and `BuildNavigationAcceleration`.
- `CellStaticData::islands` itself and every reader of it.
- `ServerBufferedFrames`' cached encoding and LZ4 framing, and
  `NetworkMessages::ServerCoordStaticDataMessage`.
- `ClientSessionRuntime::StageStaticData` and terrain preparation.
- Any client comparison of received and generated placements.
- `kiNavDataVersion`, `Frame::kiVersion`, and the save format: `NavData` and
  saved state do not change.
- `PlacementsCrc`'s computation.
- A trailing-byte check in `NetworkMessages::Read`. Chat bytes after the first
  NUL are recorded design: the `Network.md` chat row validates "the text up to
  the first NUL" (`Server/Server.cpp:579-580`).
- Trailing Hello bytes after a short or absent GUID, and `OptionalGuid`.
- `MessageReader::AtEnd`, `MessageWriter`, and every other layout.
- `Server::ClientHello`, `Server::ClientChat`, and `IsValidChatText`.
- What the client build does when it reads `kServerChat` or any other
  server-to-client message: unchanged, with no new check.
- The wording of `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`.

## Risk tier and invariants

Tier 3 under `.agents/references/risk-tiers.md`: a wire/protocol change. The
bytes an engine packet carries change, so a client and server of different
builds must be rejected at Hello by the bumped version. The terminator check
and the Hello row minimum alone would be Tier 2: they tighten checks at an
existing boundary, on the server only, with the format and the trust
unchanged.

Invariants to hold:

- No backward compatibility: one current layout, older builds rejected.
- The client still marks `kNavigationDataBuilt` after `Read`, including for an
  empty graph.
- Client and server placements for a coordinate stay equal, since both still
  come from `GenerateIslandChain`.
- Every Hello and chat packet a current client writes is still accepted; the
  layout an honest client produces for them does not change.
- The client build runs no new check on server data: its read of a string
  with no NUL behaves as it does today.
- Readers signal corruption the one way `Engine/Source/Network/AGENTS.md`
  `## Corrupt Input Policy` names.

## Acceptance criteria

1. `CellStaticData::Write` and `Read` neither write nor read a placement
   count or placement fields.
2. `kiProtocolVersion` is one higher than at the start of the change, and the
   version-history paragraph of `Network.md` names the new version and both
   changes.
3. In the server build a string with no NUL leaves the reader invalid; in
   the client build `MessageReader::NullTerminatedString` compiles to what
   it is today. `kiMinSize` of the Hello is one above `kiFixedSize`, and the
   `Network.md` `kClientHello` row says 22.
4. The BrokenEngineSandbox client and server and ModelViewer build through
   `/compile`.
5. Through `/agent-harness`, with a local server: the client connects and
   publishes an authoritative cell that has navigation data, and
   `cell_coordinate_probe` for that cell's coordinate reports the same
   `placements.crc` on client and server; a chat message the client sends is
   relayed.
6. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
   CRC mismatch.

The originating evidence records no harness setup for criteria 5 and 6; the
executing session sets them up from the harness documentation.

## Coordination

`Documents/Plans/Engine/RemoveCoordUpdateTickReader.md` and
`Documents/Plans/Engine/RemoveUnreferencedNetworkWireSymbols.md` edit
`NetworkMessages.h` or `NetworkProtocol.h` too. None of the three depends on
another. Whichever lands later re-derives the lines of its references from the
landed headers.

## Notes

- No harness command sends a malformed Hello, so the terminator rejection
  itself is verified by reading the code, not by a live run.

- `BuildPlaneNavigationData` (`Engine/Source/Frame/NavCellData.cpp`) and the
  navigation build around it compile only under `BT_SERVER`. Its comment above
  the `vertices.empty()` test cites `CellStaticData::Read` on the client.
  Re-read that comment against the changed `Read`; this Plan does not edit it,
  so if it no longer holds, surface it as a leftover under the Change Workflow.

- The byte saving is small: at most `kiMaximumIslandsPerCell` twenty-byte
  entries per packet, sent from a cached encoding. The gain is removing dead
  wire content and the discarded allocation, not bandwidth.
