<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:08.378Z","dependsOn":[]} -->
# Reject client static data with missing island templates before adoption

## Context

The accepted finding `CAI/shard-0014/001` identifies a queued-client network
boundary gap. `FrameStaticData::Read` bounds the placement count but stores an
opaque `islandCrc` without checking `IslandTerrain::mIslands`
(`Engine/Source/Frame/FrameStaticData.cpp:24-37`). `Client::Receive` parses and
queues the static-data object inside its packet catch
(`Engine/Source/Network/Client/ClientReceive.cpp:324-339`), but
`ClientSessionRuntime::ProcessReceived` applies it after that catch and
`ClientSession::ApplyReceivedStaticData` calls `AcquireTextureSlot`; the first
operation there is `mIslands.at(islandCrc)` (`IslandTerrainResidency.cpp:126-130`,
`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp:18-31`).
An absent template therefore escapes the receive boundary and terminates the
client during a normal update, contrary to the hostile-input and static-data
contracts.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The missing
membership check is unresolved, pre-existing, and outside the approved audit
work.

## Design

The author's recommendation is to validate every placement CRC against the
loaded island-template map while `ServerCoordStaticData` is still inside the
packet receive boundary, before the parsed object is queued. Reject the whole
static-data payload through the existing packet failure path when any template
is absent, leaving the current slot/frame state untouched. Keep count, epoch,
coordinate, texture-request, and later application ordering unchanged for a
fully valid payload.

## Critical files

- `Engine/Source/Network/Client/ClientReceive.cpp:324-339` — static-data parse and queue boundary.
- `Engine/Source/Network/Client/Client.cpp:255-310` — receive exception boundary.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp:18-31` — application hook.
- `Engine/Source/Frame/IslandTerrainResidency.cpp:126-130` — current throwing lookup (read-only evidence).
- `Engine/Source/Network/Client/AGENTS.md` and `Engine/Source/Network/AGENTS.md` — hostile-input/static-data contracts.

## In scope

- Island-template membership validation for each parsed static-data placement
  before queueing or adoption.
- Rejecting a semantically invalid static-data packet through the existing
  receive path without mutating the active subscription slot.
- The receive/application boundary named above.

## Out of scope

- Grid-save staging (`GridSaveIslandReference.md`), frame-area validation,
  navigation topology, or generic packet count/layout checks.
- Changing the island manifest/handshake, placement format, slot epochs,
  texture residency policy, or server packet generation.
- Adding a fallback template or changing `AcquireTextureSlot` behavior for
  valid references.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). This change validates
hostile network input before it reaches a client adoption hook outside the
packet catch.

Preserve these invariants:

- A queued static-data payload contains only island CRCs resolvable by the
  client's loaded template map.
- Invalid input is rejected without partial slot/frame adoption or an
  uncaught main-loop exception.
- Valid static-data receive order, count/epoch/coord checks, terrain requests,
  and reconciliation sequencing remain unchanged.

Tier rationale: the fix is a pre-specified membership test of each placement
CRC against the already-loaded island-template map at one receive site,
rejecting the payload through the existing packet failure path. Packet layout,
placement format, and every valid payload's handling stay exactly as they are.

## Acceptance criteria

- A fixed-size, otherwise valid static-data packet containing an absent island
  CRC is rejected inside the receive boundary, leaves the active slot unchanged,
  and does not reach `mIslands.at` during `ClientUpdate`.
- A valid packet with all template CRCs still queues, applies, and requests
  matching terrain textures exactly as before.
- Client `Debug|x64` builds clean through `/compile`.

## Coordination

`Documents/Plans/Engine/FrameAreaValidation.md` also validates parsed static
data before client publication. Keep island-template membership and area
geometry as separate predicates at the shared receive boundary, preserve the
whole-packet rejection and slot-immutability behavior, and re-derive line
citations before implementation. No dependency is required.

## Notes

The consolidated index marks this and `CAI/shard-0013/005` as probable
`DUP-004`; their network queue and server save-staging boundaries have distinct
verification strategies, so this Plan remains separate.
