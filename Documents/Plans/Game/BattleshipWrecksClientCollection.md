<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T17:04:35.353Z","dependsOn":[]} -->
# Replace the battleship wreck list with a client-only Wrecks collection

## Context

Stage S4 of the split between simulation state and client-only visual state.
It extends the `Engine/Source/Frame/AGENTS.md`
`## Simulation and Visual State` section the client-only Explosions stage
adds. The authoring session recorded as the user's choices: visual-only
state lives in the Frame's client-only part, which the engine Frame has and
the game Frame lacks (D1); wrecks are copied over from the previous snapshot
at a full resync (D5); wrecks follow the simulation while the map is open, so
a death inside map view leaves a wreck (D6); wrecks keep today's loaded-area
cell rule (D7). This Plan takes the D5 copy from the snapshot ring tail, so
the copy needs a non-empty ring, and a reset that empties the ring loses the
cell's wrecks (Design 5).

Today a wreck is file-scope render state outside every Frame
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsRender.cpp:58-204`,
the `BattleshipWreck` struct and `sWrecks` vector). `UpdateWrecks`, called
from `BeginRender` (`:206-213`), captures each visible interpolated dying row
once per `(GridCoord, battleship_t)`, then advances every wreck by the
sim-scaled render delta, so map view's world-hidden frames freeze wrecks and
a death wholly inside map view leaves none. `ClearWrecks` (`:201-204`) runs
from `Game::ResetGameState` (`Game.cpp:177`) and `ClientSession::OnServerLoad`
(`Network/Client/ClientSession.cpp:206`) because cell-local Ids restart
there. A wreck sampled at render rate is not tick-exact and cannot own Frame
rows, which the hull-explosion stage needs. The game Frame has one tuple per
half and no client-only subset (`Frame/FrameCollections.h:14-22`), so a
client-only game collection has no home yet.

Facts verified at this baseline:

- First dying tick: `BeginExplosion` sets `pDestroyedTimes` to exactly
  `kBattleshipDestroyTime` (`Battleships/BattleshipsCombat.cpp:43`) during
  combat, and the next tick's Interpolate Update decrements it
  (`Battleships.cpp:224-227`), so in the same tick's Spawn phase a row with
  `kDying`, `pDestroyedTimes == kBattleshipDestroyTime`, and no `kDestroy` or
  `kTransfer` bit is on its first dying tick. A `kDestroyShip` or harness
  `kDestroy` removal never sets that time, so it leaves no wreck, as today.
  The dying row is not compacted that tick (`Battleships.cpp:314-325`), and
  game Spawn runs after Destroy (`Engine/Source/Frame/FrameBase.cpp:362-365`),
  so its stored pose is final for the tick.
- Motion rules and constants (`BattleshipsRender.cpp:58-61`, `:131-175`,
  `:177-198`): capture samples elevation under the hull box center and four
  corners; open sea floor under all five sinks, otherwise the box bottom
  rests on the highest sample, never above the start height, tilted toward
  the terrain normal at the center by the fraction of the fall completed;
  gravity ramps over `kBattleshipWreckPowerLossTime`, then water brakes the
  fall to `kfBattleshipWreckSinkSpeed` once the center reaches z = 0; a
  sinking wreck is dropped once its hull top is below the water.
- Rendering (`BattleshipsRender.cpp:264-380`): visible wrecks take the slots
  after the rows of their coordinate and pipeline, share its skinning
  reservation, add to the buffer capacity, freeze their animation time, and
  publish the `Wrecks` counter (`kCpuCounterBattleshipWrecks`,
  `Profile/ProfileRows.h:20`); `Rendered` excludes them. Dying rows are not
  drawn.
- Allocation: the collection growth helpers already suppress allocation
  tracking (`Engine/Source/Frame/Collections/CollectionMemory.h:182`, `:324`),
  replacing the `push_back` suppression at `BattleshipsRender.cpp:171-173`.

Evidence baseline `e2279c55`; game paths are relative to
`Projects/BrokenEngineSandbox/Source/`.

Change Workflow tier: Tier 3. Trigger: the game Frame's build-local
serialization tuples, copy, compaction, and read validation change, and a new
paired collection layout is added; the shared CRC, wire, and save formats are
meant to stay byte-identical, which the acceptance criteria prove.

## Design

1. Game tuple subset, mirroring the engine's `Collections()` and
   `ServerCollections()` (`Engine/Source/Frame/FrameBase.h:142-161`). In
   `Frame/FrameCollections.h`, `GameInterpolateCollections` and
   `GamePostRenderCollections` become build-local: on the client they append
   `*rSelf.pWrecks` after Battleships. New `GameServerInterpolateCollections`
   and `GameServerPostRenderCollections` return the five shared pairs, with
   matching `GameServer*Types` aliases and a server-build `static_assert` that
   each build-local tuple equals its server tuple. In `Frame/Frame.cpp` the
   server subset replaces the build-local one in the version sum (`:105`),
   both `Crcs` (`:613`, `:663`), both `LogDifferences` (`:624`, `:675`), both
   `ServerRead` (`:653`, `:703`), and `Frame::ServerRead` pair validation
   (`:727`); every other use (allocate-and-copy, phases, compaction, render,
   `Write`/`Read`, build-local validation `:744`) keeps the build-local tuple.
   `std::unique_ptr<WrecksInterpolate> pWrecks` and
   `std::unique_ptr<WrecksPostRender> pWrecks` join `FrameInterpolate` and
   `FramePostRender` under `BT_CLIENT` (`Frame/Frame.h`), constructed with the
   others.
2. Wrecks collection, added through `/add-collection` as a whole-file
   `BT_CLIENT` pair in `Frame/Collections/Wrecks/Wrecks.h` and `Wrecks.cpp`,
   with no `kiVersion`, ids, or owned handles. Interpolate columns:
   `pVecPositions` (cell-local, W = 1), `pVecDirections`, `pfFallSpeeds`,
   `pFallTimes`, `pAnimationTimes` (frozen at capture). PostRender columns:
   `pLifecycleFlags`, `pController`, `pVecTilts` (target quaternion, identity
   when sinking), `pfStartZ`, `pfRestZ`, `pfTopOffsets`, `pbSinks`,
   `pCaptureTicks`. No `PersistentMembers()`: allocate-and-copy carries every
   column.
3. Hooks.
   - `WrecksPostRender::Spawn`: for each Battleships row on its first dying
     tick (Context), add a row from the row's stored pose, controller, and
     animation time, sampling terrain through the cell's own
     `CellElevationSampler` and `CellNormal` and the hull box from
     `BuildShipHitbox`, exactly as `UpdateWrecks` captures today, and
     storing `rFrame.interpolate.iTick` in `pCaptureTicks`.
   - `WrecksInterpolate::Update`: the fall, brake, sink, and rest step of
     `UpdateWrecks` (`BattleshipsRender.cpp:177-198`), from the previous
     frame's columns with the frame delta, correct at any delta including
     zero, so render interpolation needs nothing extra.
   - `WrecksPostRender::Update`: no work (columns copy forward).
   - `WrecksPostRender::Destroy`: `MarkDestroy` a row that has sunk, or whose
     cell no longer overlaps the camera's cell loaded area
     (`mf4CellLoadedArea`, or `mf4PreviousCellLoadedArea` while
     `mbPreviousCellLoadedAreaValid`), the `OverlapsCellLoadedArea` test of
     `BattleshipsRender.cpp:90-94` with `rStaticData.coordinate`. Tick code
     reading the camera follows existing precedent: explosion spawning
     already reads `gpCamera->mBasisCoordinate` on the tick worker
     (`Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:193`),
     and the camera changes only on the render path
     (`Engine/Source/GameBase.cpp:871-881`).
   Registered after Battleships, the Wrecks Spawn hook runs after the
   Battleships one.
4. Rendering. Battleships `BeginRender` adds the Wrecks rows to its capacity
   with `AccumulateRenderCapacity` over `pWrecks`; `RenderPipeline` takes the
   coordinate's interpolated Wrecks rows (`rFrameInterpolate.pWrecks`) and
   their PostRender rows (`gpGame->RenderFrame(coordinate).postRender.pWrecks`)
   in place of `sWrecks`, skipping `kDying` rows, and keeps the tilt
   fraction, slot, skinning, and counter rules. Wrecks declares no render
   hooks of its own.
5. Hydration (D5). `ClientSession::HydrateReceivedFullState`
   (`Network/Client/ClientSession.cpp:241-260`), when `pRingTail` is not
   null, copies the tail's Wrecks pair into the received frame with
   `engine::AllocateAndCopyMembers` for each half, then `MarkDestroy`s every
   copied row whose `pCaptureTicks` is later than the received frame's tick.
   The author recommends that filter: the tail can be ahead of the full
   state, and re-simulating from the full state recaptures any death after
   its tick, so without it a wreck would appear twice. A copied row keeps the
   tail's motion state, a few ticks ahead of the full state; the resulting
   small forward step of its fall is accepted. A client reset that empties
   the ring first loses the cell's wrecks, as it loses every other client-only
   effect: `ClientSessionBase::ResetCoordinateStatesForResynchronization`
   (`Engine/Source/Network/Client/ClientSessionBase.cpp:58-63`) calls
   `Cell::ResetClientState` (`Engine/Source/GameBase.h:141-159`), which sets
   the snapshot count to zero, so hydration finds no tail and copies nothing.
6. Removal. Delete `BattleshipWreck`, `sWrecks`, `UpdateWrecks`,
   `ClearWrecks`, and its two callers; move the four wreck constants to the
   Wrecks files. Server load clears every cell
   (`Engine/Source/Network/Client/ClientSessionBase.cpp:99`), and so does
   `CreateNewFrame` (`Engine/Source/GameBase.cpp:1080`, `:1085`); the
   executing session confirms each `ResetGameState` path discards the cell
   frames before deleting the call there.

## Critical files

- `Frame/FrameCollections.h`, `Frame/Frame.h`, `Frame/Frame.cpp`
- new `Frame/Collections/Wrecks/Wrecks.h`, `Wrecks.cpp`
- `Frame/Collections/Battleships/BattleshipsRender.cpp`, `Battleships.h`
- `Network/Client/ClientSession.cpp`, `Game.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `.filters` (new files, client project only)
- `Engine/Source/Frame/AGENTS.md`

## In scope

- The game tuple subset and its consumers in `Frame/Frame.cpp` (Design 1);
  the `pWrecks` members and constructors.
- The Wrecks pair, its columns, and its Interpolate Update, PostRender
  Update, Destroy, and Spawn hooks (Design 2-3).
- Battleships `BeginRender`, `RenderPipeline`, and `Render` wreck handling
  (Design 4).
- `HydrateReceivedFullState` Wrecks copy (Design 5).
- Deletion of the wreck list, `ClearWrecks`, and its callers (Design 6).
- Documentation: `Frame/Collections/Battleships/AGENTS.md`
  `## Wreck Invariants` rewritten for the collection (capture tick, motion
  in the tick, Destroy rule, hydration copy, map view following the
  simulation); `Frame/AGENTS.md` tuple invariant gains the client-only Wrecks
  entry and the server subset; `Engine/Source/Frame/Collections/AGENTS.md`
  `## Runtime Rules` loses the `(GridCoord, Id<T>)` keyed-state sentence,
  whose only user this stage removes;
  `Documents/Architecture/FrameUpdatePipeline.md` where it lists game
  collections or phase participation; `Engine/Source/Frame/AGENTS.md`
  `## Simulation and Visual State`, so its simulation-state bullet also names
  the `GameServerInterpolateCollections` and `GameServerPostRenderCollections`
  collections and its visual-state bullet also names the collections the game
  build-local tuples add to them.

## Out of scope

- Hull explosions and every Battleships death column (the next stage).
- Other ship deaths, death duration, and the wreck model or motion tuning.
- Any change to the shared CRC, wire, grid-save, or replay formats.
- Wreck persistence across a resynchronization that resets the ring.
- Backward compatibility.

## Coordination

- `Documents/Plans/Engine/CullOffscreenUnownedEffectsAtSpawn.md` also edits
  `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`, adding the
  off-screen bullet. The two stages change different bullets; whichever lands
  second rebases its bullet onto the other's.

## Acceptance criteria

- Builds: `/compile` BrokenEngineSandbox client and BrokenEngineSandboxServer
  Debug|x64. ModelViewer Debug|x64 only if an engine header in its project
  changes; the design changes none.
- Layout: the collection-layout checker `/add-collection` documents passes.
- Shared format unchanged: `Frame::kiVersion` is unchanged, a replay recorded
  before the change still plays (`replay_play`) with no CRC mismatch, and a
  connected client shows no desync (`get_logs` `[Dd]esync` returns nothing)
  during a battleship death.
- Wreck: a combat death of a random-controller Battleship leaves one wreck
  that falls and rests or sinks as before (screenshots at the death, after
  one second, and at rest); a harness `kDestroy` leaves none. The author
  recommends lowering its `pfHealths` with `edit_frame` and flying an enemy
  `SpawnBlaster` into it; no harness run has exercised this setup.
- Map view: a death while the map is open leaves a wreck visible after the
  map closes, advanced by the elapsed time.
- Cell rule: moving the camera so the wreck's cell leaves the loaded area
  removes the wreck; returning does not restore it.
- Hydration: a full state adopted while the ring is retained neither drops
  nor duplicates an existing wreck (`Wrecks` counter stays 1).
- Counters: the profile `Wrecks` counter counts drawn wrecks and `Rendered`
  excludes them.

## Notes

- Originating gap: the user-directed simulation/visual split; wrecks need a
  tick-exact home that can own Frame rows.
- Exposure: game Frame serialization tuples (build-local only), threading
  (Wrecks hooks run on the per-cell worker and read only their own cell and
  the immutable camera), client project membership, live verification.
- Determinism: wrecks are client-only and outside the CRC; a rollback
  regenerates them from the snapshot.
