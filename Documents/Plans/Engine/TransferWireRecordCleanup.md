<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T16:59:23.415Z","dependsOn":[]} -->
# Transfer Wire Record Cleanup: Remove the Dead Columns in One Batch

## Context

The cross-cell transfer record carries several columns that are written and, in
some cases, read back, but whose values can never influence the arrived entity.
Each was proven individually during
`Documents/Plans/Engine/TransferProducerShapeDecision.md`, which named them in
its own `## Out of scope` (`:63`). They are batched here because every one of
them costs the same wire-version bump and the same cross-cell determinism
verification, and paying that once is cheaper than paying it five times.

Dead item 1 — blaster wind-trail tuning. `SerializeBlasterTransfer` writes
`fWindTrailIntensity`, `fWindTrailWidth`, and `fWindTrailLengthMultiplier`
(`Engine/Source/Network/NetworkSerialization.cpp:19-22`) and
`DeserializeBlasterTransfer` reads all three back (`:86-88`), but arrival
overwrites all three with tweak defaults regardless of what arrived:
`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:36-39`, whose own comment
states the reset is deliberate ("Wind-trail tuning is client-only visual debug
state; reset to canonical defaults on server-authored transfer"). This matches
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/AGENTS.md`
("Cross-cell transfer resets client-only wind-trail tuning to canonical client
defaults"), so the documented behavior and the code already agree — the wire
carry is what is redundant.

Dead item 2 — player shield animation state. `SerializePlayerTransfer` writes
`fShieldRotation` and `fShieldShrink`
(`NetworkSerialization.cpp:69-70`, read back at `:134-135`), but
`PlayersPostRender::Spawn` at arrival deliberately omits both so they take fresh
defaults; the comment recording that is `SpawnTransfer.cpp:91` ("Shield
rotation/shrink are client-only visual animation state; reset to canonical
defaults while gameplay transfer fields restore verbatim"), and
`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md` states the
same rule for arrivals generally.

Dead item 3 — Blasters' direction fill. `BlastersPostRender::Transfer` fills
`request.data.vecDirection` from `pVecDirections[i]`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/
Blasters.cpp:190`), but `SerializeBlasterTransfer` never writes a direction, and
`BlastersPostRender::Spawn` re-derives it at arrival as
`XMVector3Normalize(rInfo.vecVelocity)` (`Blasters.cpp:132`). The fill is a
local dead store on the blaster path only.

Dead item 4 — request-only bookkeeping fields.
`TransferRequest::iEntityId`
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.h:85`) has exactly one writer,
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/
PlayersNavigation.cpp:270`, and no reader anywhere.
`TransferRequest::iPushedTick` (`Frame.h:88-89`) is written by all four
producers — `Spaceships.cpp:407`, `Blasters.cpp:195`,
`PlayersNavigation.cpp:271`, `Missiles.cpp:368` — and read by nobody; its
comment ("Every live entry should match the current tick") describes an
expectation nothing checks. `TransferRequest` is an in-memory frame structure,
not a wire record, so these two cost no version bump on their own.

Dead item 5 — the shared-member column list. `TransferData::SharedMembers()`
(`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:95` and `:98`) lists
`fWindTrailIntensity`, `fWindTrailWidth`, `fWindTrailLengthMultiplier`,
`fShieldRotation`, and `fShieldShrink`, so the same dead values also participate
in the structure's shared comparison surface.

## Design

Remove all five items in one change, in this order, then verify once.

1. Delete the three `fWindTrail*` writes (`NetworkSerialization.cpp:19-22`) and
   reads (`:86-88`), and drop `3 * kiF32` from the `kTransferBlaster` row of
   `StatusChangeItemWireSize` (`:163`).
2. Delete the `fShieldRotation`/`fShieldShrink` writes (`:69-70`) and reads
   (`:134-135`), and reduce the `kTransferPlayer` row from `10 * kiF32` to
   `8 * kiF32` (`:166`).
3. Delete the five now-unused members from `TransferData` (`StatusChange.h:129-131`
   and `:142-143`) and from `SharedMembers()` (`:95`, `:98`), and delete the
   `SpawnTransfer.cpp:36-39` overrides and the `:91` comment those members
   existed for — arrival then reaches the same defaults by simply not naming the
   fields, exactly as the player path already does.
4. Delete the `.vecDirection` line at `Blasters.cpp:190`. Nothing else changes
   on that path: arrival already re-derives direction at `Blasters.cpp:132`.
5. Delete `TransferRequest::iEntityId` (`Frame.h:85`) with its single writer
   (`PlayersNavigation.cpp:270`), and `TransferRequest::iPushedTick` with its
   comment (`Frame.h:88-89`) and its four writers (`Spaceships.cpp:407`,
   `Blasters.cpp:195`, `PlayersNavigation.cpp:271`, `Missiles.cpp:368`).
6. Bump `engine::kuiProtocolVersion`
   (`Engine/Source/Network/NetworkProtocol.h:63`) and
   `game::FrameInput::kiVersion`
   (`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:11`) once for the
   whole batch, per `Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`.

Explicitly kept: the `common::ValidateVector<false>` call on
`rRequest.data.vecDirection` inside `game::PushTransferRequest`
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:740`). Only the Blasters
producer stops filling `vecDirection`; Spaceships, Missiles, and Players still
fill and serialize it, so that validation stays live and must not be removed
along with item 4.

## Critical files

- `Engine/Source/Network/NetworkSerialization.cpp`
- `Engine/Source/Network/NetworkProtocol.h`
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h`
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/AGENTS.md`

## In scope

- The six numbered steps above, each confined to the exact lines they name.
- `StatusChangeItemWireSize`'s `kTransferBlaster` and `kTransferPlayer` rows,
  which are the receive-side mirror of the removed writes
  (`NetworkSerialization.cpp:144-148`).
- Reviewing `kiMaxStatusChangeBytesPerItem`
  (`Engine/Source/Network/NetworkSerialization.h:18`) as that same comment
  requires; it is an upper bound a shrinking payload cannot invalidate, so a
  change there is only permitted if review proves one is needed.
- Whatever `AGENTS.md` sentences the removals make inaccurate — at minimum the
  wind-trail reset sentence in
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/AGENTS.md`
  and the arrival-reset sentence in
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`, both of
  which describe a reset that becomes an omission.
- Explicit user consent for the replay/save compatibility break, obtained in
  the session that implements this.

## Out of scope

- `TransferData::smokeTrailId`, its wire bytes, and the SmokeTrails documented
  invariant. That is
  `Documents/Plans/Engine/MissileSmokeTrailCarryReachability.md`, which this
  Plan depends on so the two wire changes do not collide and the version bump
  is paid once.
- The `common::ValidateVector<false>` direction check in
  `game::PushTransferRequest` — still reachable from three producers.
- `vecDirection` fills in Spaceships, Missiles, and Players.
- `TransferRequest` layout beyond deleting the two named fields, the
  `transferRequests` buffer location, and the
  `PrepareTransferRequest`/`PushTransferRequest` helper shape.
- Any other `TransferData` column, the transfer LOG texts, and the arrival
  grace/transfer-marker behavior.
- Backward compatibility for older replays or saves.

## Risk tier and invariants

Tier 3. Trigger: network wire layout, `StatusChange`/`TransferData` payload
format, and the save/replay compatibility gate all change together. The removed
columns sit inside `SharedMembers()`, the server wire/CRC subset
(`Engine/Source/Frame/Collections/AGENTS.md`), so client and server must be
rebuilt and verified as a pair — a one-sided build fails the handshake by
design (`Engine/Source/Network/AGENTS.md`). Cross-cell transfer is the path
where the server recomputes each destination frame's `sharedCrc` and the client
recomputes after replaying transfers
(`Engine/Source/Network/Server/AGENTS.md`, `Engine/Source/Network/Client/
AGENTS.md`), so determinism verification must cover an actual crossing, not
just startup.

## Acceptance criteria

- Client and server both build.
- A client connects and completes the handshake against the rebuilt server.
- Blasters, missiles, spaceships, and a player each cross a cell boundary with
  no CRC mismatch and no desync logged.
- Blaster wind-trail appearance and player shield animation after a crossing are
  unchanged from before the change.
- Replay determinism passes on a capture recorded after the version bump.
- No reference to the five removed `TransferData` members or the two removed
  `TransferRequest` members remains anywhere in the tree.

## Notes

Source: proven out-of-scope residuals from the session implementing
`Documents/Plans/Engine/TransferProducerShapeDecision.md`. Sequenced after
`Documents/Plans/Engine/MissileSmokeTrailCarryReachability.md` through
`dependsOn`; if that Plan's Branch B lands first and already bumps both
versions, this Plan still bumps them again — the two changes are separate
incompatible wire revisions.
