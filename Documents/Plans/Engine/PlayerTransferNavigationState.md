<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T21:39:19.350Z","dependsOn":[]} -->
# Preserve Player navigation state across cell transfers

## Context

The confirmed finding shows that `PlayersPostRender::Transfer` copies the
Player position, heading, velocity, timers, flags, identity, fleet wanted
coordinate, and pending ticks, but omits the wanted direction, cached AI
steering direction, and island destination
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:241-276`).
`TransferData` has no members for those three vectors, and the Player wire
serializer/deserializer and per-item width mirror do not carry them
(`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:90-164`;
`Engine/Source/Network/NetworkSerialization.cpp:47-66,103-147`).

`SpawnTransfer` forwards the available payload to `PlayersPostRender::Spawn`
(`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:75-97`).  Destination
materialization then sets wanted direction from the hull direction, zeros AI
steering and island destination, and overwrites the navigation mode and
waypoint bits with fresh-spawn defaults
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:459-483`).
These values are shared members and CRC contributors
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:269-320`),
so the next fixed tick can follow a different aim and navigation path even
though both endpoints apply the same reset.

The governing collection contract requires a transfer arrival to restore
carried gameplay state verbatim except the documented arrival-grace and
client-only defaults (`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:13`).
Player navigation is shared-state-driven and cached steering is expected to
survive skipped pathfinding ticks
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/AGENTS.md:6-18`).
`PlayerFlags` already stores navigation mode and waypoint index in
`uiPlayerFlags` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:149-195`),
and the producer masks only `kTransfer` before sending those bits
(`PlayersNavigation.cpp:267`).  The existing `SpawnInfo::bTransfer` marker
selects transfer values from genuine-spawn defaults
(`Players.h:327-353`; `SpawnTransfer.cpp:75-97`).

Impact: every ordinary cross-cell Player transfer currently retargets aim,
discards cached steering and island destination, and restarts navigation.  The
loss is visible in shared deterministic state and can alter destination CRCs,
network reconciliation, and replay outcomes.

## Design

Author's recommendation: extend the existing Player transfer arm with exactly
three `XMVECTOR` members, in this order adjacent to the existing transfer
vectors: `vecWantedDirection`, `vecAiDirection`, and `vecIslandDestination`.
Add them to `TransferData::SharedMembers`, copy them from the three
`PlayersPostRender` columns in `PlayersPostRender::Transfer`, serialize and
deserialize them only in the existing `kTransferPlayer` codec arm, and pass
them through `SpawnTransfer` into `PlayersPostRender::SpawnInfo`.
When `bTransfer` is true, `Spawn` should materialize all three vectors
verbatim.  It should preserve the navigation mode and waypoint bits already
present in `rInfo.flags` by applying `SetNavDirection` and
`SetNavWaypointIndex` fresh-spawn defaults only when `bTransfer` is false.
The producer continues to strip only `kTransfer`; no duplicate navigation-mode
or waypoint fields are added.

Keep genuine Player spawn defaults, random consumption, arrival grace,
transfer lock, existing non-navigation transfer fields, and the client-only
debug waypoint default unchanged.  Normal saves contain already-materialized
collection columns rather than a `TransferData` payload, so they do not gain a
new save surface.

This is one current format: raise the Player item-width expression and shared
maximum for the three added vectors (the Player payload remains 152 bytes),
increment the current `engine::kuiProtocolVersion` once for the incompatible
wire layout, and increment the current `FrameInput::kiVersion` once because
replay stores raw `StatusChange` payload bytes.  Increment the current
`PlayersPostRender::kiVersion` once because its shared CRC inputs now
participate in the corrected transfer state; `Frame::kiVersion` already
composes that contributing version and does not need an independent base bump.
Do not add a compatibility shim or accept the old wire/replay payload under
the new versions.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:241-289` — Player transfer producer and the existing `kTransfer` mask.
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:90-164` — `TransferData` fields, shared equality, and raw replay payload shape.
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:75-97` — Player transfer-to-`SpawnInfo` materialization.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:149-195,198-320,327-353` — navigation flag packing, CRC-visible columns, `SpawnInfo`, and `PlayersPostRender::kiVersion`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:459-491` — transfer versus genuine-spawn initialization.
- `Engine/Source/Network/NetworkSerialization.cpp:47-66,103-147,195-197,357-359` — Player transfer encode/decode and wire-width mirror.
- `Engine/Source/Network/NetworkSerialization.h:18-23` — shared per-item and batch-size bounds.
- `Engine/Source/Network/NetworkProtocol.h:63` — protocol compatibility version.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:9-14` — replay input compatibility version.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:15-18` — composed deterministic-frame version (reference; keep its base unchanged).
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:13` and `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/AGENTS.md:6-18` — transfer and Player navigation authority.
- `Projects/BrokenEngineSandbox/Source/Network/AGENTS.md:14-16` and `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md:18-20` — wire and replay versioning contracts.

## In scope

- Carrying `vecWantedDirection`, `vecAiDirection`, and
  `vecIslandDestination` through the existing `kTransferPlayer` producer,
  `TransferData`, codec, `SpawnTransfer`, and `SpawnInfo` path.
- Preserving every existing `uiPlayerFlags` bit except the transient
  `kTransfer` marker, including navigation mode and waypoint index, while
  retaining fresh defaults for genuine spawns.
- Updating `TransferData::SharedMembers`, the Player wire-width/max
  accounting, `engine::kuiProtocolVersion`, `FrameInput::kiVersion`, and
  `PlayersPostRender::kiVersion` as described above; keep the composed
  `Frame::kiVersion` base unchanged.
- A focused deterministic cross-cell Player verification using the existing
  replay/transfer harness.  If `replay_transfer_fixture` and `query_players`
  cannot expose the complete tuple, add only the smallest temporary
  verification seam needed to observe the three vectors and packed navigation
  bits before transfer and immediately after materialization, then remove that
  seam before the final implementation lands.

## Out of scope

- Genuine Player spawn behavior, fresh navigation/waypoint defaults, random
  draw policy, arrival-grace or transfer-lock semantics, and client-only debug
  waypoint or visual defaults.
- Navigation algorithms, pathfinding cadence, steering updates, fleet policy,
  or any new status-change type or permanent product query API.
- Other transfer arms, unrelated Player timers or identity fields, normal save
  layout, `.pack` data, or compatibility readers/shims for the old format.
- Unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3.  Trigger: the change adds shared
deterministic gameplay state to a cross-cell transfer, changes the network wire
layout and raw replay `StatusChange` payload, and changes CRC-visible state
across client/server and replay integration.

Preserve these invariants:

- A Player transfer arrival restores the three carried vectors and all
  non-transient packed flags bitwise, including navigation mode and waypoint
  index; only the documented arrival-grace, transfer-lock, and client-only
  defaults differ.
- `bTransfer` remains the explicit materialization marker.  A genuine spawn
  still receives its existing hull-aim, zero-steering, zero-destination,
  flagship/roaming-mode, zero-waypoint, and client debug-waypoint defaults.
- The Player transfer codec's write order, read order, width mirror, maximum
  bound, protocol gate, and replay version stay paired; non-Player transfer
  arms retain their current wire fields.
- The corrected destination state contributes identically to client and server
  CRCs and to record/replay simulation.

## Acceptance criteria

- A deterministic cross-cell Player scenario with non-default wanted aim, AI
  steering, island destination, navigation mode, and waypoint index captures
  the complete tuple immediately before `PlayersPostRender::Transfer` and
  observes bitwise-equal carried vectors and packed navigation bits immediately
  after destination materialization; only the documented transfer exceptions
  differ.
- Genuine Player spawn in the same verification run retains its existing
  defaults, including a zero client-only debug waypoint, and no fresh-spawn
  default overwrites a transferred navigation bit.
- The scenario produces matching client/server CRCs after materialization, and
  recording then replaying the same transfer completes with no checksum,
  read, or desynchronization errors.
- The Player wire codec round-trips the expanded current-format payload, the
  new `kuiProtocolVersion` and `FrameInput::kiVersion` gates reject the old
  format through existing compatibility handling, and no compatibility shim is
  introduced.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

The investigation's frozen source candidate is `CPT/shard-0049/003` at audit
commit `80896f33661aaab99cf180a96db54600099be652`.  A live-plan search found no
duplicate owner for this transfer-verbatim navigation/aim root cause;
`PlayerFireTimerValidation.md` and the other transfer Plans cover independent
timer, ownership, or input-validation boundaries.

## Coordination

`Documents/Plans/Engine/SpaceshipTransferBehaviorFlags.md` independently
changes `TransferData`, the Spaceship per-arm codec width (65 bytes after its
added flag), and the protocol/replay version gates; that arm does not by itself
require raising the current 120-byte shared maximum.  No dependency is
required.  Whichever Plan lands second must re-derive the current field order
and each per-arm width from the then-current baseline, verify the shared
`kiMaxStatusChangeBytesPerItem` remains sufficient for the resulting largest
arm, and raise it only if that largest arm exceeds the then-current maximum,
while preserving the fields and version changes from the first Plan.  This
Plan's 152-byte Player arm independently requires and informs its shared-
maximum change; it must not hard-code a next version or reorder the Spaceship
arm's fields.
