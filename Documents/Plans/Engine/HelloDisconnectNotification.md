<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:23.199Z","dependsOn":[]} -->
# Publish disconnects for rejected accepted peers

## Context

The retained survivor `CAI/shard-0037/001` identifies a server/game cleanup
gap. The three incompatible-Hello branches in
`Engine/Source/Network/Server/ServerReceive.cpp:256-293` send a rejection and
call `RemoveClient` directly without adding `PendingDisconnect`. A later ENet
disconnect cannot repair the record because `Server::Disconnect` publishes a
notification only when `FindClient` still returns it
(`Engine/Source/Network/Server/Server.cpp:164-177`). The game consumes that
notification to clear ownership and the connected GUID mapping at
`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:172-192`
and `ServerFleetManager.cpp:393-400`.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0037.md:67`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:957`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source
was changed by routing. Repeated Hello is contract-valid after handshake, so
the missing game notification is reachable with a fixed-size valid packet.

## Design

The author's recommendation is to route incompatible-Hello rejection for an
already accepted peer through one notification-before-removal helper that
captures the stored GUID and client identity first. Keep the initial
pre-handshake rejection notification-free because it owns no game state, and
preserve exactly one game disconnect notification for an accepted peer.

## Critical files

- `Engine/Source/Network/Server/ServerReceive.cpp:256-335` — Hello gates and
  accepted-peer identity.
- `Engine/Source/Network/Server/Server.cpp:164-177,526-543` — normal and
  violation disconnect publication.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:172-192`
  and `ServerFleetManager.cpp:393-400,481-493` — game cleanup consumers.
- `Engine/Source/Network/Server/AGENTS.md` and
  `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` — removal and
  persistent-fleet contracts.

## In scope

- Disconnect notification publication in the post-handshake incompatible
  Hello branches.
- Ordering of GUID capture, pending notification, peer removal, and later ENet
  disconnect handling.
- Preservation of the pre-handshake rejection path.

## Out of scope

- Hello packet layout, protocol/data validation rules, or GUID uniqueness.
- Fleet persistence policy, ownership relinking, and unrelated contract
  violation thresholds.
- Client disconnect UI or discovery behavior.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: this crosses
network handshake validation, transport record lifetime, and server/game
ownership cleanup.

Tier rationale: the Design fully specifies one notification-before-removal
helper used by the three rejection branches, reusing the existing
`PendingDisconnect` publication. Only the incompatible-Hello rejection path
changes; no packet layout, protocol field, or valid-session behavior is
touched.

Preserve these invariants:

- Every removed accepted client produces exactly one game disconnect
  notification before its transport record disappears.
- Persistent fleet state remains while the connected-client mapping and owned
  entity records are cleared.
- An unaccepted peer's incompatible Hello remains notification-free.

## Acceptance criteria

- A valid repeated Hello with an incompatible protocol, Frame, or pack token
  publishes one pending disconnect carrying the stored GUID before removal.
- The game clears that client's ownership/mapping, and later ENet disconnect
  does not publish a duplicate notification.
- Initial rejected handshakes and ordinary disconnect/violation paths retain
  their existing notification behavior.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0037/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:957`. No source fix or build
was performed during routing.
