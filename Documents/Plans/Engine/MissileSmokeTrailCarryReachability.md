<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T16:58:29.234Z","dependsOn":[]} -->
# Missile Smoke-Trail Carry: Confirm Reachability, Then Fix or Delete

## Context

`Engine/Source/Frame/Collections/SmokeTrails/AGENTS.md:9` documents that
"Cross-cell missile transfer reuses the trail identity and bypasses that spawn
delay so the rebound trail continues immediately." Static tracing performed
during `Documents/Plans/Engine/TransferProducerShapeDecision.md` indicates the
reuse path can never fire:

- Producer fill is client-only: `Missiles.cpp:364-366` sets
  `.smokeTrailId = rCurrentInterpolate.puiSmokeTrails[i]` under
  `#if defined(BT_CLIENT)`; `StatusChange.h:176-179` declares the member itself
  only under `BT_CLIENT`, so a server build has no such member.
- The server therefore writes a literal `0` on the wire:
  `Engine/Source/Network/NetworkSerialization.cpp:48-53`
  (`#else int64_t iSmokeTrailId = 0;` then `WriteInt64`).
- The client deserializer unconditionally overwrites whatever the local
  `TransferData` held with that wire value:
  `NetworkSerialization.cpp:115-118`.
- `0` is invalid identity (`engine::uuid_t::IsValid`,
  `Engine/Source/Frame/Collections/CollectionId.h:32-35`, wrapped by
  `id_t::IsValid` at `:75`), so `SmokeTrailsPostRender::Add`
  (`Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsUpdate.cpp:41-72`)
  always takes the `else` branch: a fresh visual id plus a fresh
  `pfStartTimes` entry, which is exactly the spawn-suppression the documented
  invariant says transfer bypasses.
- Nothing else reaches the reuse path with a live id. `game::SpawnTransfer` is
  called from exactly two sites — `Engine/Source/Network/Client/
  ReconcileReplayTick.cpp:144` (from received `FrameInput` status changes) and
  `Engine/Source/Network/Server/ServerTransferManager.cpp:142` — and the
  client never spawns from its own `postRender.transferRequests`, which it
  reads only to detect local-player cell migration
  (`Projects/BrokenEngineSandbox/Source/Network/Client/
  ReconcileReplayClientState.cpp:73-86`).

The two owning documents also contradict each other, which is why this needs
settling rather than a silent edit. `SmokeTrails/AGENTS.md:9` says identity is
reused across transfer; `Projects/BrokenEngineSandbox/Source/Frame/Collections/
Missiles/AGENTS.md` says "Live transfer may receive a new smoke-trail identity
on arrival because smoke identity is client-local", and the sibling
`Blasters/AGENTS.md` states outright that blaster trail identity is not carried.
Under the repository authority order, documentation outranks current code
behavior, so the code cannot be treated as the answer by itself.

This is a static conclusion only. It was never observed at runtime, so the Plan
starts by confirming behavior before changing anything.

## Design

Step 1 — confirm at runtime. Use `/agent-harness` to run a client-observed
missile crossing a cell boundary and watch its smoke trail across the crossing.
The observable question is single and binary: does the trail visibly restart
(a break or a re-grow from zero length) at the crossing?

Step 2 — take exactly one branch from that observation. No further judgment is
required; the observed answer selects the branch.

Branch A — trail restarts (the documented invariant is real and broken). Make
the smallest change that lets a live trail identity survive the crossing, and
keep it inside the existing mechanism rather than inventing a new one. The
constraint that forces the design is that identity must reach the client that
renders the arrival, while the server has no such member and must occupy
identical wire space (game Network AGENTS.md: "Client-only values still occupy
identical server-side wire space"). Resolve that within the existing client
smoke-continuity hydration path owned by `game::ClientSession`
(`Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`, "per-frame
gameplay hydration and smoke continuity") rather than by making the server
carry client-only identity. If the confirmation shows the fix must change the
wire record itself, stop and surface that for re-planning instead of expanding
scope here.

Branch B — trail does not restart (the field and the documented invariant are
dead). Delete the dead carry end to end:

- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h` — the
  `smokeTrailId` member (`:176-179`) and the `BT_CLIENT` arm of `operator==`
  plus its build-asymmetry comment (`:109-120`).
- `Engine/Source/Network/NetworkSerialization.cpp` — the write (`:48-53`) and
  the read (`:115-118`), and the `kiI64` term in `StatusChangeItemWireSize`'s
  `kTransferMissile` row (`:165`).
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:71-73` — the
  `BT_CLIENT` `.smokeTrailId` initializer.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/
  Missiles.cpp:364-366` — the producer fill.
- `Engine/Source/Frame/Collections/SmokeTrails/AGENTS.md:9` — the sentence
  claiming transfer reuses trail identity, leaving the new-trail suppression
  statement intact and consistent with `Missiles/AGENTS.md`.

Branch B removes wire bytes, so it also bumps `engine::kuiProtocolVersion`
(`Engine/Source/Network/NetworkProtocol.h:63`, currently `11`) and
`game::FrameInput::kiVersion`
(`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:11`, currently `15`),
and breaks replay/save compatibility for existing captures — which needs
explicit user consent in that session under the repository's
backward-compatibility directive.

Deliberately left alone in both branches: whether the transfer wire record
carries other dead columns. That is
`Documents/Plans/Engine/TransferWireRecordCleanup.md`, which depends on this
Plan so the version bump and cross-cell determinism verification are paid once.
`SmokeTrailsPostRender::Add`'s `reuseId` parameter itself stays: explosions and
missile respawn use the same entry point, and Branch B removes only the
transfer carry, not the reuse capability.

## Critical files

- `Engine/Source/Frame/Collections/SmokeTrails/AGENTS.md`
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsUpdate.cpp`
- `Engine/Source/Network/NetworkSerialization.cpp`
- `Engine/Source/Network/NetworkProtocol.h`
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h`
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/AGENTS.md`

## In scope

- Runtime confirmation through `/agent-harness`: a client-observed missile
  crossing a cell boundary, recording whether its smoke trail restarts.
- Exactly one of Branch A or Branch B above, confined to the files listed under
  `## Critical files`.
- Branch A: the client-side smoke-continuity hydration in
  `game::ClientSession`, plus whichever of `SmokeTrails/AGENTS.md:9` and
  `Missiles/AGENTS.md` must change so the two stop contradicting each other.
- Branch B: the exact deletion list in `## Design`, the two version bumps, and
  the same documentation reconciliation.

## Out of scope

- The other dead transfer wire and request fields —
  `fWindTrailIntensity`/`fWindTrailWidth`/`fWindTrailLengthMultiplier`,
  `fShieldRotation`/`fShieldShrink`, the Blasters `vecDirection` fill,
  `TransferRequest::iEntityId`, `TransferRequest::iPushedTick`. Those belong to
  `Documents/Plans/Engine/TransferWireRecordCleanup.md`.
- The `reuseId` parameter of `SmokeTrailsPostRender::Add` and its explosion
  caller.
- `TransferRequest` layout, the `transferRequests` buffer location, and the
  `PrepareTransferRequest`/`PushTransferRequest` helper shape.
- Blaster or explosion trail identity behavior.
- Adding backward-compatibility handling for older replays or saves.

## Risk tier and invariants

Tier 3. Trigger: Branch B changes the network wire record and the
save/replay-compatibility gate; Branch A touches cross-cell transfer arrival
behavior on the client. Either way the change reaches the transfer path that
`Frame::sharedCrc` is recomputed around
(`Engine/Source/Network/Server/AGENTS.md`, `Engine/Source/Network/Client/
AGENTS.md`), so client/server determinism across a crossing must hold. The
`smokeTrailId` member is client-only and outside `SharedMembers()`
(`StatusChange.h:89-107`), so it does not itself enter the CRC; the wire-size
row and the two version constants do gate compatibility.
`kiMaxStatusChangeBytesPerItem`
(`Engine/Source/Network/NetworkSerialization.h:18`) is an upper bound that a
shrinking payload cannot invalidate, but the comment at
`NetworkSerialization.cpp:144-148` requires reviewing it with any payload
change.

## Acceptance criteria

- The harness run is recorded with its observation, and the branch taken
  matches it.
- Client and server both build.
- A client-observed missile crosses a cell boundary with no CRC mismatch and no
  desync logged, and replay determinism still passes.
- Branch A: the trail no longer restarts at the crossing.
- Branch B: no `smokeTrailId` reference remains outside
  `SmokeTrailsPostRender::Add`'s own `reuseId` mechanism, and
  `SmokeTrails/AGENTS.md` and `Missiles/AGENTS.md` agree.

## Notes

Source: proven out-of-scope residual from the session implementing
`Documents/Plans/Engine/TransferProducerShapeDecision.md`, which names the
`smokeTrailId` asymmetry in its own `## Out of scope` (`:81`, `:91`). The
conclusion above is static only and has never been observed running, which is
why Step 1 exists.
