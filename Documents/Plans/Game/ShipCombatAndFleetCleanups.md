<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:48:39.758Z","dependsOn":[]} -->
# Ship combat and fleet cleanups

## Context

An audit of commits cfd6116e, 13e4ae19, and 5c2e6a8d proved no reachable defect in
the game code but found the cleanups below, each proven by reading: a duplicated
block, a dead branch, or a guard for a state that cannot occur. They are
pre-existing relative to that session's documentation change, which took no C++.

Paths are under `Projects/BrokenEngineSandbox/Source/`, and line numbers are from
5c2e6a8d, except those in `ShipHitboxes.cpp`, which are from the concave outline
bake's landing. The item labels are local to this Plan.

## Design

Each item is independent unless it says otherwise. Apply them in one change,
re-reading each proof at its site first, and drop any item whose proof no longer
holds, reporting it. Items G02 and G04 start with the check each states.

Names below are the author's recommendations.

## Critical files

- `Frame/Collections/Battleships/BattleshipsCombat.cpp`
- `Frame/Collections/Missiles/Missiles.cpp`, `MissilesUpdate.cpp`
- `Frame/Collections/Ships/ShipBehaviours.cpp`, `ShipHitboxes.cpp`
- `Network/Client/ClientSession.cpp`
- `Network/Server/ServerClientManager.cpp`, `ServerFleetManager.cpp`
- `FleetSelection.h`, `FleetSelection.cpp`, `Ui/Screens/HudScreen.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.h`

## In scope

- G01 Hardpoint lookup. `Frame/Collections/Battleships/BattleshipsCombat.cpp:312-319`
  and `Network/Server/ServerClientManager.cpp:251-261` repeat the chunk lookup,
  offset math, and pack-layout `ASSERT`s on every launch. Resolve and assert once
  per controller in `InitializeShipHitboxes`, cache `std::span` tubes and bays on
  `ShipHitboxShape`, and reduce each site to reading the span, dropping its
  `File/PackChunks.h` include. Only Battleship models carry hardpoints, so the
  nonzero tube `ASSERT` stays Battleship-only. The spans borrow eager chunk data;
  confirm it outlives the frames that read `gShipHitboxShapes`.
- G02 Destroy-member send. `SendDestroyFleetMemberRequest`
  (`Network/Client/ClientSession.cpp:226-251`) repeats the client-null and
  connected guards, the workbuffer write, and the flush that
  `ClientSessionRuntime::SendGameRequest` owns, and skips its log tick scope.
  First resolve the conflict with `Network/AGENTS.md`, which requires a
  `GameMessages` payload's write and read to route through its one `Visit`:
  `SendGameRequest` takes arithmetic arguments, so its one-line use would bypass
  `Visit`. The author recommends keeping the rule, because it exists to stop the
  two sides drifting, and adding a `ClientSessionRuntime` send that takes a message
  and writes it through `engine::NetworkMessages::Write`, sharing the guards, tick
  scope, and flush with `SendGameRequest`. The other option is passing
  `globalId.iValue` to `SendGameRequest`, as the siblings do, and narrowing the
  rule's text to say so. The wire bytes stay
  `DestroyFleetMemberRequestMessage::kiSize` plus the type byte either way.
- G03 Exhaust light type. `MissilesInterpolate::Register`
  (`Frame/Collections/Missiles/Missiles.cpp:192-216`) registers two identical
  exhaust types. Register one `siExhaustAreaLightTypeIndex` and remove the
  selections at `:116` and `:165`.
- G04 Upper missile plane. First re-prove no production writer of
  `MissileFlags::kUpperFlightPlane`. `BattleshipsCombat.cpp:345` sets it from the
  stored plane, so the proof covers every write of `puiFlightPlanes` on a
  Battleship row: `Battleships.cpp:252` stores what `ClimbTowardsFlightPlane`
  leaves, and that function can raise its target by one plane when the lower plane
  is blocked (`Frame/Collections/Ships/ShipBehaviours.cpp:116`); `Battleships.cpp:412`
  stores `rInfo.uiFlightPlane`. If a Battleship can reach a stored plane above
  `kiBattleshipHighestFlightPlane`, drop this item and report the path. If not,
  remove the flag, `GetMissileFlightPlane`, the `Transfer` line
  (`Missiles.cpp:298`), the static assert beside it, and the `flightPlane` field
  (`Agent/AgentCommandsServerQueries.cpp:106`). Use the lowest plane at the two
  `MissilesUpdate.cpp` reads (`:62`, `:124`), through
  `engine::gpFlightPlanes->Lowest()` where a height is wanted. Leave the other
  `MissileFlags` values unchanged so stored bytes keep their meaning. Reduce the
  assigned-plane text in `Frame/Collections/Missiles/AGENTS.md`,
  `Frame/Collections/Ships/AGENTS.md`, `Frame/AGENTS.md`, and
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`.
- G05 Pending destroy marks. `FleetSelection::mDestroyMemberControls` holds a
  `NetworkUiControl<bool>` per member to track owned-entity presence, but every
  request reaching the server erases the member and sends a sync, which already
  prunes the map; the refusal paths are replay, where the button is disabled or a
  load clears `FleetSelection`. Replace it with
  `std::unordered_set<int64_t> mPendingDestroyMemberIds`: insert on click, test for
  disable, and keep the existing prune and clear. Remove the `Reset` block in
  `ClientSession::ApplyOwnedEntityEvent`, both `Update` calls, and the per-frame
  `try_emplace` in `HudScreen.cpp:281-296`. Move the allocation suppression to the
  insert.
- G06 Height step. The missile step (`MissilesUpdate.cpp:62-78`) is the same
  three-branch clamp as `ClimbTowardsFlightPlane` (`ShipBehaviours.cpp:120-134`).
  Extract `StepHeightTowards(fHeight, fTarget, fStep, rbArrived)` in `ShipBehaviours`.
- G07 Destroy guards. `Network/Server/ServerFleetManager.cpp:149-164` nests guards
  for states a live member cannot reach: `mFrameInputs` is rebuilt from an active
  set computed just before (`Engine/Source/GameBase.cpp:1126-1127`),
  `DetectFleetMemberDeaths` runs after every tick, and `ShipStatusChanges.cpp:79`
  ignores a dying row. Use `.at` for the frame input, dereference
  `FindShipByGlobalId`, and push unconditionally. Update the "present, live,
  non-dying row" sentence in `Network/Server/AGENTS.md`. Because a client request
  reaches this code, re-prove the member's coordinate is current on the tick the
  request drains, and drop the item if it is not.
- G08 Death terrain contact. `Fighters.cpp:671` and `Omnis.cpp:739` test
  `IsFlightPlaneAllowed` for the upper plane, always true for both types by
  `kiFighterHighestFlightPlane` and `kiOmniHighestFlightPlane`, and the same guard
  in `RaiseAboveTerrain` (`ShipBehaviours.cpp:144-147`) is never taken because only
  those two types call it. Delete the clause and the guard, and move the rest of
  the test into `TakesDeathTerrainContact(fHeight, iTargetPlane)`.
- G09 Plane recovery. The loop at `ShipBehaviours.cpp:153-161` needs a terrain
  sample plus the lowest plane's height to equal an effective height bit-exactly,
  and the next `ClimbTowardsFlightPlane` stores the plane on arrival anyway. Always
  store `kuiNoFlightPlane`, and change the exact-height sentence in
  `Frame/Collections/Ships/AGENTS.md`.
- G10 Fighter rotation restore. After G08, `fNavigationDeltaRotation` and its
  restore (`Fighters.cpp:669,677-680`) are no-ops: only a dying row bounces, and
  `bAligning` is set only on the live branch. Delete both, and the "preserving that
  navigation turn through terrain response" clause in `Ships/AGENTS.md`.
- G11 Heading rotation. `ServerClientManager.cpp:271,277-280` and
  `BattleshipsCombat.cpp:330,339-342` write the rotation four times and the
  flatten, normalize, and rotate block twice. Add one `RotateByHeading` in
  `ShipBehaviours`.
- G12 Climb fraction. `kfFighterClimbPercentage`, `kfOmniClimbPercentage`, and
  `kfBattleshipClimbPercentage` are single-use and equal. Replace them with one
  `kfShipClimbSpeedFraction` in `Frame/Collections/Ships/ShipBehaviours.h`, which
  all three type headers include.
- G13 Contact assert. Delete `ASSERT(std::isfinite(fContact))`
  (`Frame/Collections/Ships/ShipHitboxes.cpp:128`): `fEnter` and `fExit` are finite
  for finite positions, `fExit` keeps its finite initial value only for a zero
  delta, and the `std::min` with `fExit` bounds `FirstOutlineCrossing`'s infinite
  no-crossing result.
- G14 Unread intensity. Delete `.fLightingIntensity` at
  `Frame/Collections/Fighters/Fighters.cpp:116`: `AreaLightsRender.cpp:104` prefers
  the `pLightingIntensityWrapper` that registration sets.
- G15 Column lists. Each `Fields()` and `Values()` body at
  `Frame/Collections/Battleships/Battleships.h:294`, `Blasters/Blasters.h:112`,
  `Fighters/Fighters.h:299`, `Missiles/Missiles.h:206`, `Omnis/Omnis.h:362`,
  `Frame/StatusChange.h:95,111,124`, and `Frame/Frame.h` repeats its column list.
  Use `return {};`.
- G16 Missile validator name. Rename `ValidateFlyingMissilePlanes`
  (`Frame/Frame.cpp:77,746`) to `ValidateFlyingMissiles` and fix its "plane"
  failure strings.
- G17 Clearance tier parameter. Compute `ShipTypeClearanceTier(eType)` inside
  `ClimbTowardsFlightPlane`, as `ResolveTerrainContact` does, and drop the
  parameter from its three callers.
- Stale comment: `Graphics/Camera.cpp:80` names a Time of Day slider; the control
  is labeled Sun Angle Override.

## Out of scope

- `ASSERT(bHit)` at `ShipHitboxes.cpp:116`, which stays: every ship model's
  prism holds its origin (`Ships/AGENTS.md` `## Ship Invariants`).
- The `PackedModelMissileTube` rename:
  `Documents/Plans/Engine/EngineExportRenderAndViewerCleanups.md` owns it.
- The Omni heal beam's `.fLightingIntensity` at `Omnis.cpp:193`, which has no
  wrapper and is live.
- Any change to Battleship flight planes or missile plane behavior.
- New validation, packet types, or wire fields.

## Risk triggers and invariants

- Tier 3. Trigger: determinism/CRC. G09 changes a stored CRC'd value on an exact
  height tie, so it takes the bump `Frame/AGENTS.md` `## Invariants` requires for
  changed CRC semantics. G02 also spans Engine and game network code.
- G04 removes a bit no production path sets. Its column, wire payload, and
  `MissileFlags` values stay, so it needs no layout, `FrameInput::kiVersion`, or
  `engine::kiProtocolVersion` change. `/add-collection-member` is not triggered.
- G02 keeps the packet bytes, so no protocol bump.
- G01 reads startup-cached pack data on both builds; simulation results stay
  bit-identical.
- G06, G08, G10, G11, G12, and G17 preserve operation order in fixed-tick code.
  G12 keeps each climb rate's value.
- G05 is client-only UI state. Its allocation stays suppressed and bounded by
  fleet members.
- G07 is on a client-to-server path: failing loudly must rest on server state, never
  on a client value, and the existing ownership check stays.

## Acceptance criteria

- `/compile` builds the BrokenEngineSandbox client and server.
- `/agent-harness` replay determinism check passes: a recorded replay plays back
  with no CRC mismatch, using a recording made after the `Frame::kiVersion` bump.
- G04's handoff states the re-proof, or that the item was dropped and why.
- G02's handoff states which resolution was taken, and
  `Network/AGENTS.md` matches the code.
- A live `/agent-harness` run: the fleet member `[x]` button retires a Fighter once,
  shows pending until the fleet sync, and is disabled during replay; Battleships
  launch missiles and Fighters. The audit read source only: the supplied evidence
  records no harness run or setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

G01 and `Documents/Plans/Engine/EngineExportRenderAndViewerCleanups.md` touch the
same hardpoint reads. Either order works: the later landing uses whichever struct
name and site the tree then has.
