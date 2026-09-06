<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:50:03.976Z","dependsOn":[]} -->
# Send retained fleets when a reconnect has no live player relink

## Context

The accepted finding `CAI/shard-0054/001` identifies a reconnect publication
gap.  `OnClientDisconnected` clears only the connected-client mapping and
retains `mFleets` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:393-400`).
On a valid reconnect, `ServerClientManager::NewClients` calls
`RelinkFromFrames`; only a positive player count invokes `OnClientConnected`.
The zero-player branch marks the client processed and sends no FleetSync
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:40-78`).
The client has already cleared its local FleetSelection on connection loss,
and the Hello path has no alternate FleetSync (`Projects/BrokenEngineSandbox/Source/Game.cpp:527-555`;
`Engine/Source/Network/Server/ServerReceive.cpp:315-340`).

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0054.md:62`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1179`.
Assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the reconnect gap is pre-existing,
unresolved, and outside the audit work.

Impact: the server retains an empty or all-dead owner fleet that the reconnecting
client cannot see, so the owner cannot name it for respawn or management.

## Design

Author's recommendation: after a valid handshake with a known persistent
owner GUID, invoke the existing fleet refresh/sync path whenever that owner
entry exists, regardless of whether `RelinkFromFrames` found live players.
Keep the positive relink refresh behavior, processed-client gating, and no-fleet
case unchanged; invoke the existing sync method for a known owner even when its
retained vector is empty so the client receives the canonical empty state
representation.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:40-78` — reconnect branch and processed-client state.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:369-400` — fleet refresh/sync and disconnect retention.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.h` and `ServerFleetManager.h` — existing reconnect APIs.
- `Engine/Source/Network/Server/ServerReceive.cpp:315-340` — Hello path evidence.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:527-555` — client FleetSelection reset evidence.

## In scope

- Reconnect-time FleetSync publication for known owners with retained empty or
  all-dead fleets and zero live-player relink results.
- Reusing the existing `OnClientConnected`/`SendFleetSyncToClient` lifecycle
  without changing FleetGuid identity or spawn request semantics.
- Preserving processed/dead/waiting client gates and positive relink refresh.

## Out of scope

- Fleet save parsing, FleetGuid validation, flagship repair, duplicate owner
  rows, client FleetSelection UI, or a new reconnect protocol message.
- Changing disconnected-fleet retention, member liveness rules, or spawn queue
  assignment.
- Replaying or reconstructing player rows that `RelinkFromFrames` did not find.

## Risk tier and invariants

Expected Change Workflow Tier 3.  Trigger: persistent network ownership and
reconnect lifecycle must publish authoritative fleet state across server/client
boundaries.

Preserve these invariants:

- Every accepted reconnect with a known owner receives its retained FleetGuid
  vector even with zero live players; no-fleet owners remain valid.
- Positive relink, member refresh, death/flagship handling, and FleetSync wire
  layout remain unchanged.
- No simulation CRC, save/replay format, or transfer identity changes.

## Acceptance criteria

- Disconnect a client owning an ordinary empty fleet, reconnect with the same
  persistent GUID, and observe FleetSync containing that fleet before the client
  can issue fleet controls.
- Repeat with a retained fleet whose members are all dead; the vector remains
  visible and addressable for respawn.
- Reconnect with at least one live relink and with no retained fleets; existing
  positive and no-fleet behavior remains unchanged.
- Client and server `Debug|x64` builds clean through `/compile`; a reconnect
  scenario observes the sync without an unrelated fleet-create request.

## Notes

The consolidated index records no duplicate-family or external-claim hint for
this reconnect-specific publication gap.
