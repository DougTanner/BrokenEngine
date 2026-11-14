<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:38:52.840Z","dependsOn":[]} -->
# Require the terminator of a client string

## Context

Paths are under `Engine/Source/Network/` unless they start elsewhere. Verified
against commit `10d84ca6`; the lines cited are those at commit `c675b6b2`.

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

Require the terminator, in the server build only:

- `MessageReader::NullTerminatedString` clears `mbValid` when it reaches the
  packet end without a NUL (`NetworkMessages.h:254-257`), in a branch guarded
  by `#ifdef BT_SERVER`. `NetworkMessages::Read` then throws, as it does for
  every other invalid layout, and the server's dispatch catch drops the
  packet and counts the violation.
- `ClientHelloMessage::kiMinSize` becomes `kiFixedSize + 1`
  (`NetworkMessages.h:556`).
- The `kClientHello` row of `Documents/Architecture/Network.md` changes Min 21
  to 22.
- `engine::kiProtocolVersion` increments by one from the literal the
  executing session finds, and the version-history paragraph of `Network.md`
  (after the contract table) gains that version.

Rationale: root `AGENTS.md` `## Key Patterns` says the server fully validates
every client-to-server record, and the policy section is the more general
rule. Recording the terminator as optional in `Network.md` was rejected for
that reason.

The version increments because `AGENTS.md` `## Transport Contracts` bumps
"a change to what an engine packet's contract row requires of its payload
content", and this change raises the `kClientHello` row's minimum size and
makes the terminator a payload requirement of the Hello and chat rows. The
bullet's stated reason, that a client of another build would be rejected,
does not occur here, because no build writes a 21-byte Hello; the bullet's
rule is followed as written.

Placement, decided: the one guarded branch in the reader. The server build
reads only client strings through this call, the `kClientHello` build
configuration (`NetworkMessages.h:574`, `Server/Server.cpp:615-616`) and the
`kClientChat` text (`NetworkMessages.h:752`, `Server/Server.cpp:577-578`), so
one branch covers both and the check never runs on data the server wrote.
The client build reads `kServerChat` through the same call
(`NetworkMessages.h:768`, `Client/Client.cpp:854-855`); the guard compiles
the check out there, so the client accepts exactly what it accepts today.
That follows `AGENTS.md` `## Corrupt Input Policy`: client code adds no new
structural checks on server data. Every `Visit`, and so each paired layout,
stays shared and unguarded.

A check in each of the two server handlers instead was rejected: it is two
sites repeating one comparison of the string's end against the packet end,
and it puts a layout check in handlers that hold only semantic validation.

## Critical files

- `Engine/Source/Network/NetworkMessages.h`
- `Engine/Source/Network/NetworkProtocol.h` (`kiProtocolVersion`, `:83`)
- `Documents/Architecture/Network.md`

## In scope

- `MessageReader::NullTerminatedString`: the no-NUL exit only, as one
  `BT_SERVER`-guarded branch.
- `ClientHelloMessage::kiMinSize`.
- One increment of `kiProtocolVersion`.
- The `kClientHello` row and the version-history paragraph of `Network.md`.

## Out of scope

- A trailing-byte check in `NetworkMessages::Read`. Chat bytes after the first
  NUL are recorded design: the `Network.md` chat row validates "the text up to
  the first NUL" (`Server/Server.cpp:579-580`).
- Trailing Hello bytes after a short or absent GUID, and `OptionalGuid`.
- `MessageReader::AtEnd`, `MessageWriter`, and every other layout.
- `Server::ClientHello`, `Server::ClientChat`, and `IsValidChatText`.
- What the client build does when it reads `kServerChat` or any other
  server-to-client message: unchanged, with no new check.
- The wording of `AGENTS.md` `## Corrupt Input Policy`.

## Risk tier

Tier 3 under `.agents/references/risk-tiers.md`. Trigger: the
`kiProtocolVersion` increment changes the wire/protocol surface, because
builds on either side of it no longer connect. The tightened check and the
row minimum alone would be Tier 2: they tighten checks at an existing
boundary, on the server only, with the format and the trust unchanged.

Invariants to hold:

- Every packet a current client writes is still accepted; the wire layout an
  honest client produces does not change.
- The client build runs no new check on server data: its read of a string
  with no NUL behaves as it does today.
- Readers signal corruption the one way `## Corrupt Input Policy` names.

## Acceptance criteria

1. In the server build a string with no NUL leaves the reader invalid; in
   the client build `MessageReader::NullTerminatedString` compiles to what
   it is today. `kiMinSize` of the Hello is one above `kiFixedSize`, and the
   `Network.md` row says 22.
2. `kiProtocolVersion` is one above its value before the change, and the
   version-history paragraph of `Network.md` names the new version.
3. The BrokenEngineSandbox client and server and ModelViewer build through
   `/compile`.
4. Through `/agent-harness`, a client connects to a local server and a chat
   message is relayed. The originating evidence records no harness setup; the
   executing session sets it up from the harness documentation.

## Coordination

`Documents/Plans/Engine/RemoveCoordUpdateTickReader.md`,
`Documents/Plans/Engine/RemoveUnreferencedNetworkWireSymbols.md`, and
`Documents/Plans/Engine/SeparateNetworkProtocolHeaderResponsibilities.md`
edit `NetworkMessages.h` or `NetworkProtocol.h` too. The last moves the
identifier half of `NetworkProtocol.h` (`:1-112`) into a new
`NetworkProtocolIdentifiers.h`, and that half holds one symbol each of the
other three edits: `PacketTypeName` (`:46-81`), `kiProtocolVersion` (`:83`),
and `kiMaximumPacketSize` (`:108`). None of the four depends on another.
Whichever lands later re-derives the file as well as the lines of its
references from the landed headers: once the header Plan has landed,
`NetworkProtocolIdentifiers.h` is the critical file for those three symbols.

## Notes

- No harness command sends a malformed Hello, so the rejection itself is
  verified by reading the code, not by a live run.
