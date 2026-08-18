<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:48:34.167Z","dependsOn":[]} -->
# Server-to-Client Timescale Packet

## Context

The server-to-client timescale update and client mirror currently live in the
game packet range: Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:345-388
and Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:70-94.
The client-to-server
GamePacketType::kClientTimespeedRequest is debug/game policy; the
GamePacketType::kServerTimespeedUpdate is engine-shaped.

The unresolved D12 item is whether only the server-to-client update becomes an
engine packet. The current protocol is version 10. Inserting an engine packet
below kGamePacketStart shifts every game-range numeric ID, so an old peer
could misparse the 16-byte payload unless the Hello gate rejects it first.

## Design

The researched option is to promote only the server-to-client update below
kGamePacketStart, retain the client request in the game packet contract, and
bump kuiProtocolVersion from 10 to 11. The bounded alternative is to keep both
IDs game-owned until a separate protocol decision; do not partially promote
the feature or silently reuse an existing ID.

If promotion is selected, update the current packet-contract table in
Documents/Architecture/Network.md and preserve the FEC feature as an
independent manual item.

## Critical files

- Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:345-388
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:70-94
- Projects/BrokenEngineSandbox/Source/Network/GameMessages.h
- Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h
- Documents/Architecture/Network.md
- The current protocol-version declaration and Hello admission gate

## In scope

- The server-to-client timescale packet ownership and numeric ID decision.
- The corresponding client decode and server send path.
- Protocol version bump and Hello rejection when promotion is selected.
- Architecture packet-table documentation for the then-current protocol.

## Out of scope

- Game-owned client timescale requests and debug policy.
- Forward-error-correction work in Documents/Features/Network.
- Any D1 codec/transfer extraction or unrelated packet changes.
- Compatibility readers for protocol 10 after the gate rejects them.

## Risk tier and invariants

Expected Change Workflow Tier 3: this changes a wire enum, protocol version,
and client/server admission behavior. Preserve payload width and meaning,
packet ordering, Hello mismatch refusal, every shifted game ID, and the
current timescale update semantics. Protocol version 10 remains current until
the selected promotion is implemented.

## Coordination

No directional prerequisite is required. This Plan may be bundled with the
change-list transport work only if both implementation records explicitly
retain their separate acceptance and protocol decisions; D1 is not a
dependency. The Network architecture document must describe only the selected
current protocol.

## Acceptance criteria

- The implementation record states whether the promotion or retention option
  was selected.
- If promoted, a version-11 peer sends and receives the exact existing
  timescale payload, while a version-10 peer is rejected before packet decode.
- All shifted game packet IDs are covered by the packet-contract check and no
  old ID is silently reused.
- If retention is selected, return the Plan for explicit user-authorized
  rejection; retention alone cannot satisfy completion acceptance, and no
  packet enum, protocol version, or documentation bytes change before that
  rejection.
- Client and server compile and the existing connection/timescale scenario
  shows no new admission or decode errors.

## Notes

The FEC rider is intentionally not scheduled by this Plan.
