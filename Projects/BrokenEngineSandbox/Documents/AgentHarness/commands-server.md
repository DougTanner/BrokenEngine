# BrokenEngineSandbox Server Commands

[Back to AgentHarness hub](../AgentHarness.md)

### Server commands

The engine-owned server commands are in the
[command reference](../../../../.agents/skills/agent-harness/references/command-reference.md#server-commands).
BrokenEngineSandbox takes over `status`, `load`, and `replay_transfer_capture`,
adding the fields below; the other entries are its own commands.

- `status`: takes over the engine
  [`status`](../../../../.agents/skills/agent-harness/references/command-reference.md#server-commands),
  adding `{"pendingFlagshipUpdateCount":int,"pendingAgentStatusChangeCount":int}`.
  `pendingFlagshipUpdateCount` counts restored or runtime-queued flagship
  navigation updates waiting for an advancing update.
  `pendingAgentStatusChangeCount` counts every queued `inject_payload` entry,
  spawn entries and destroy and update entries alike, including entries
  scheduled for a later `tick` until a drain releases them.
- `load`: takes over the engine
  [`load`](../../../../.agents/skills/agent-harness/references/command-reference.md#server-commands),
  adding `{"pendingFlagshipUpdateCount":int}`, as `status` reports it after the
  load. The game frame version a save must match is this project's
  `Frame::kiVersion`.
- `replay_transfer_capture`: takes over the engine
  [`replay_transfer_capture`](../../../../.agents/skills/agent-harness/references/command-reference.md#server-commands),
  adding `omniCount`, `spaceshipCount`, `blasterCount`, and `missileCount` to
  each `events` entry, each entry's counts summed across the destinations of
  that tick. A spawn, whether an `inject_payload` or a fleet spawn, counts under
  its collection's count, as a transfer does; a `SpawnExplosion` counts under
  none, so an event holding only explosion spawns has every count zero.
- `query_frame`: `{"coord":[x,y]}` for a loaded, ready cell. Returns counts under `omnis`, `spaceships`, `missiles`, and `blasters`.
- `query_omnis`: `{"coord":[x,y],"offset"?:0,"limit"?:256}`. A negative `offset` or `limit` is a parameter error. Returns `{"total","omnis":[{"index","uuid","globalId","local":[x,y,z],"dir":[x,y,z],"armor","shield","flags":int,"lifecycleFlags":int,"alignment"}]}`. Every row belongs to the requested cell, so `local` is meters from that cell's center and no row repeats the coordinate.
- `query_collection`:
  `{"coord":[x,y],"collection":"spaceships"|"missiles"|"blasters","offset"?:0,"limit"?:256}`.
  A negative `offset` or `limit` is a parameter error. Returns
  `{"total","items":[...]}`. Spaceship rows:
  `index,local,dir,health,deltaRotation,alignment,registryId`; missile rows:
  `index,local,dir,deltaRotation,deltaRotationDelay,alignment,registryTargetId`;
  blaster rows: `index,local,dir,alignment`. `local` is meters from the
  requested cell's center. `registryId` is the spaceship's frame registry handle
  and `registryTargetId` is the registry handle the missile is homing on, so a
  live missile's nonzero `registryTargetId` matches a spaceship `registryId` in
  the same cell, or one that spaceship cleared later in the same tick (death,
  destroy, or transfer) and the missile drops at its next Update; `0` means no
  target. `deltaRotation` is the live turn rate; a missile's
  `deltaRotationDelay` counts down through zero and stays negative after its
  launch ramp finishes.
- `edit_frame`: `{"coord":[x,y],"edits":[...]}`; these are the only top-level
  keys. Overwrites chosen values in the cell's current frame, the frame
  `query_frame` reads, at the command drain, paused or not; the next tick
  simulates from the edited frame. Each edit is
  `{"collection":"omnis"|"spaceships"|"missiles"|"blasters"|"explosions"|"pushers","index":int,"writes":{...}}`
  or `{"collection":"frame","writes":{...}}`, with no other keys. A collection
  edit's `index` is a row with `0 <= index < count`, the collection's row count
  (`query_frame` reports it for omnis, spaceships, missiles, and blasters;
  `read_frame` reports it as `count`, and an out-of-range index fails with an
  error naming it), and its `writes` keys may name any column of that
  collection's Interpolate and PostRender structs; a `frame` edit takes no
  `index` and its keys are `fSpawnTimer`, `randomEngine`, `uiNextUuid`,
  `uiFrameIdentifier`, `uiPinned`, `alignments`, `enemyAlignment`, and
  `omniAlignment` (`uiPinned` nonzero pins the cell and 0 releases it, as the
  engine
  [`create_cell`](../../../../.agents/skills/agent-harness/references/command-reference.md#server-commands)
  describes);
  `iTick` and `fDeltaTime` are named but refused as clock values. Collection
  keys are the C++ member names, verbatim, as listed in
  `Agent/Commands/ServerFrameEdit.cpp`; `frame` keys are each frame struct's
  `Values()` entries, in `Engine/Source/Frame/FrameBase.h` and `Frame/Frame.h`;
  client-only columns are not listed and fail. Edits apply in request order.
  Values by member type: `float` a finite number within the `float` range;
  `std::chrono::duration<float>` a finite number of seconds within the `float`
  range; `engine::SimulationTime` a JSON integer number of nanoseconds within
  `int64_t`, on the clock where a frame's time is its `iTick` times one tick's
  nanoseconds (31,250,000 at 32 Hz); `int32_t`, `uint8_t`, `uint16_t`, and
  `uint64_t` a JSON integer in the type's range; `XMVECTOR` `[x,y,z]`, keeping
  the element's current w; a flags column an integer within its underlying
  type; `engine::AlignmentIdentifier` an integer within `uint32_t`;
  `engine::GlobalId` an integer within `int64_t`;
  `engine::ClientGuid` `[high,low]`; `engine::GridCoord` an `[x,y]` grid
  coordinate; the `explosions` `pTrailTimes` array exactly
  `kiMaxExplosionTrails` values; `randomEngine` a nonzero integer within
  `uint64_t`; `alignments` an array of `{"a","b","flags"}` objects (`a` and `b`
  integers within `uint32_t`, `flags` an integer within `uint8_t`: 1 enemies, 2
  allies, 3 both) that replaces the alignment table, rebuilt in sorted order, so
  an empty array clears it. `edit_frame` never adds or removes a row itself: an
  agent adds rows through `inject_payload` spawn entries and removes one by
  writing its kill state (below), or for an omni by an `inject_payload`
  `DestroyOmni` entry. Identity and handle columns (`engine::Id<T>`, such as
  `pIds` and `pPushers`) are refused.

  `uiFrameIdentifier`, `pGlobalIds`, and `pClientGuids` are written unchecked: a value that duplicates another frame's or omni's identity gives odd behavior but no assert. Positions are also unchecked and must stay inside the cell: an out-of-cell omni position transfers at most one cell per axis per tick and then asserts in `OmnisPostRender::Spawn` ([Frame invariants](../../../../Engine/Source/Frame/AGENTS.md)).

  The edits apply to a staged copy of the frame, which is committed through the
  save reader's checks (pair count/capacity parity and ID maps, the
  `puiTypeIndices` bound, the `PostRead` hooks, the spawn-timer bound, and the
  one-to-one pairing of omni and spaceship pusher handles with Pushers rows, and
  no lifecycle bit on a Pushers row) plus one more refusal: a changed
  `uiNextUuid` must lie within `[current, 2^47]`.
  Any failure before the commit returns `ok:false` with the frame unchanged. On
  success the frame CRC is recomputed. The command fails without `kbDebugInput`,
  while recording, during playback, and while a `replay_record` or `replay_play`
  transition is pending; queued `inject_payload` entries do not block it.
  Returns `{"edited":int,"resynchronizedClients":int}`, `edited` counting the
  written keys.

  Removing rows: write a row's kill state and the next unpaused tick's Destroy
  phase removes it in the
  [shared compaction](../../../../Engine/Source/Frame/Collections/AGENTS.md#core-contract)
  that ends the phase, exactly as a natural death ends; an omni's or spaceship's
  pusher goes with it. Removal is swap-and-pop, so another row may move into the
  removed slot: read indices again after the tick.

  | Collection | Writes that remove the row at the next tick | Normal staged death instead |
  |---|---|---|
  | `omnis` | `pLifecycleFlags:2` (`kDestroy`) | `pfArmors:0`: PostCollision starts the 0.7 s death sequence with its explosions |
  | `spaceships` | `pLifecycleFlags:2` (`kDestroy`) | `pLifecycleFlags:4` (`kDying`), `pDestroyedTimes:0.25`, `pDestroyedExplosionTimes:0.024`: the 0.25 s staged explosions, without the first full-size explosion and death sound a killing hit adds; missiles keep homing until removal. `pfHealths:0` alone changes nothing until the next hit, and health regenerates while a live omni is more than 60 m away |
  | `missiles` | `pLifecycleFlags:2` (`kDestroy`) | none: `pLifecycleFlags:4` (`kDying`), `pDestroyedTimes:0.35` only holds the missile still and shrinks it for 0.35 s before removal; its explosion and area damage come only from a hit |
  | `blasters` | `pLifecycleFlags:2` (`kDestroy`) | none |
  | `explosions` | `pLifecycleFlags:2` (`kDestroy`); a row with trails takes its trails with it. Also `pLifecycleFlags:4` (`kDying`) on a row without trails (`piTrailCounts` 0, the `SpawnExplosion` `iTrailCount` default) | `pLifecycleFlags:4` on a row with trails: the row goes when its last trail ends. A row with neither bit, such as a `SpawnExplosion` injected without `lifecycleFlags`, is never removed unless one is written |
  | `pushers` | removed with their owning omni or spaceship only | |

  - A `pFlags` or `pLifecycleFlags` write replaces the whole mask, so include
    any bit the scenario still needs (for omnis, Update rebuilds the `pFlags`
    navigation bits).
  - With a handshaken client the commit resynchronizes clients before the tick,
    as every `edit_frame` does. A fleet-owned omni removed this way is detected
    as a death after that tick (`ServerSessionBase::RebuildOwnedEntities`). Its
    fleet then respawns it under the same global id after the 1 s delay
    (`ServerFleetManager::OnOwnedEntityDeath`, `TickRespawnTimers`), the
    ordinary death path.

  With a handshaken client, each successful edit runs the load-time
  resynchronization (`resynchronizedClients` counts those clients): it advances
  the server's load generation, which shares a 255-per-process budget with
  `load` and `reset`, zeroes `status.harvestedTransferTotal`, drops every queued
  `inject_payload` entry, and makes clients log `ClientSession::OnServerLoad`,
  drop their coordinate state, and resubscribe. So batch edits into one command.
  The resynchronization frees every subscription before clients resubscribe, so
  the server's active set keeps only cells with an omni, pinned cells, and the
  origin: with a handshaken client, an edit to an omniless cell that is neither
  the origin nor pinned in its current frame or staged copy fails before the
  commit, with the frame unchanged, and, as on `load`, every other omniless
  unpinned subscribed cell resets after a successful edit. So an edit that
  releases an omniless cell's pin with a client connected drops that cell unless
  a client resubscribes to it first. Once the budget is exhausted, the `ASSERT`
  in `Server::AdvanceLoadGeneration` throws after the edit is committed and the
  CRC recomputed: the response is `ok:false` over an applied edit, and clients
  are not resynchronized. Without a client nothing is resynchronized and queued
  entries survive.

  Recipe: `load {"pauseAfterLoad":true}` or `pause {"paused":true}`, then `create_cell` when the scenario needs an isolated empty cell, then `edit_frame`, then `pause {"paused":false}`; edit before `inject_payload`, because with a handshaken client the resynchronization drops queued entries, as `load` and `reset` do. Edits are never recorded into a replay; recording an edit into the replay stream could be built if an agent needs edits inside a recorded scenario.
- `read_frame`: `{"coord":[x,y],"collection":<name>,"index"?:int}`; these are
  the only top-level keys. Reads one row or the frame-wide values of the cell's
  current frame, the frame `query_frame` reads, and changes nothing.
  `collection` is one of the `edit_frame` collection names or `frame`. A
  collection read requires `index` with `0 <= index < count` and returns
  `{"count":int,"index":int,"values":{...}}`, `count` being the collection's row
  count and `values` holding one key per column of that collection, identity and
  handle columns included. A `frame` read takes no `index` and returns
  `{"values":{...}}`, one key per `edit_frame` `frame` key plus the clock values
  `iTick` and `fDeltaTime`. The keys are the `edit_frame` keys;
  key order is not meaningful. Each value takes the `edit_frame` form for its
  member type, except that `XMVECTOR` returns `[x,y,z]` with no w, an
  `engine::Id<T>` column such as `pIds` or `pPushers` returns its uuid integer,
  `iTick` returns an integer, and `alignments` returns the stored table in
  sorted order with `a <= b` in each pair. The command fails without
  `kbDebugInput`; recording, playback, and a pending `replay_record` or
  `replay_play` transition do not refuse it.
- `inject_payload`: `{"entries":[...],"pauseAfterWriterInput"?:bool}`; these are
  the only top-level keys, and `entries` is a non-empty array. It queues spawns
  into a cell's post-tick batch, the batch that also carries that tick's
  cross-cell arrivals, and omni destroy and update status changes into a cell's
  tick input. The whole batch validates before anything is queued or any global
  id is minted, so a rejected batch changes no pending count and leaves
  `status.nextGlobalId` unchanged. Each entry is
  `{"coord":[x,y],"type":<type>,"tick"?:int,...fields}` with a `type` from the
  table below; a key other than `coord`, `type`, `tick`, and that type's fields
  fails the batch. Field keys are the `Fields()` member tokens of the type's
  payload, verbatim: the collection's `SpawnInfo` for a spawn type (`Frame/Collections/<Collection>/<Collection>.h`; Explosions in
  `Engine/Source/Frame/Collections/Explosions/Explosions.h`) and the destroy and
  update payloads in `Frame/StatusChange.h`. Values by member type: `bool` a
  JSON boolean; `int64_t` a JSON integer; `uint8_t` a JSON integer in
  `[0,255]`; `float` a finite number; `std::chrono::duration<float>` a finite
  number of seconds; `engine::GridCoord` an `[x,y]` grid coordinate; `XMVECTOR`
  an `[x,y]` pair of finite numbers in the cell's local frame, a position being
  meters from the cell's center: `vecPosition` takes z at the lowest flight
  plane and w 1, every other vector z 0 and w 0; `engine::AlignmentIdentifier`
  `"omni"` or `"enemy"`, the game's two alignments; a flags member the whole
  mask as an integer within its underlying type, replacing the default mask;
  `engine::GlobalId` is refused, because the server mints it.

  | `type` | Fields |
  |--------|--------|
  | `SpawnOmni` | `vecPosition`, `vecDirection`, `vecVelocity`, `alignment`, `fArmor`, `fShield`, `nextBlasterFireTime`, `nextSecondarySpawnTime`, `shieldCooldown`, `shieldDownSoundCooldown`, `animationTime`, `flags`, `transferLockTimer`, `arrivalGracePeriod`, `navigationDelay`, `globalId` (refused), `fleetWantedCoordinate`, `uiPendingFleetWantedCoordinateTicks`, `uiPendingWeaponModeTicks`, `bTransfer` |
  | `SpawnSpaceship` | `vecPosition`, `vecDirection`, `vecVelocity`, `alignment`, `fHealth`, `nextBlasterSpawnTime`, `arrivalGracePeriod`, `fDeltaRotation` |
  | `SpawnBlaster` | `vecPosition`, `vecVelocity`, `iTypeIndex`, `alignment`, `fWindTrailIntensity`, `fWindTrailWidth`, `fWindTrailLengthMultiplier` |
  | `SpawnMissile` | `vecPosition`, `vecDirection`, `vecVelocity`, `vecStoredDirection`, `fAcceleration`, `flags`, `alignment`, `deltaRotationDelay`, `fDeltaRotation`, `fDeltaRotationMaximum`, `fPitch`, `time`, `nextJitter`, `bTransfer` |
  | `SpawnExplosion` | `iTypeIndex`, `vecPosition`, `vecDirection`, `flags`, `lifecycleFlags`, `iTrailCount`, `fTrailAngle`, `iParticleCount`, `fParticleAngle`, `fSizePercent`, `fSmokePercent`, `fTimePercent` |
  | `DestroyOmni` | `iOmniUuid` (required) |
  | `UpdateOmni` | `iOmniUuid` (required), `bUseMissiles`, `navigationDelaySeconds`, `uiPendingWeaponModeTicks` |
  | `UpdateFleet` | `iOmniUuid` (required), `bIsFlagship`, `fleetWantedCoordinate` (required), `uiPendingFleetWantedCoordinateTicks` |

  `iOmniUuid` is a `query_omnis` `uuid`, which changes when the omni crosses
  cells. An omitted field takes its payload struct default (for example an
  omni's `flags` 4, `kBlasterSpawnLeft`, and `navigationDelay` 60), except
  that `vecPosition` defaults to the cell center; `vecDirection`,
  `vecVelocity`, and a missile's `vecStoredDirection` to `[1,0]`; `alignment` to
  `"omni"`; an omni's `fleetWantedCoordinate` to `coord`; a `SpawnMissile`'s
  `flags`, when the entry has no `flags` key, to `kTargetEnemy` (16) for an
  `"omni"` `alignment` and to `kTargetOmni` (8) otherwise; and the `UpdateOmni`
  `uiPendingWeaponModeTicks` and `UpdateFleet`
  `uiPendingFleetWantedCoordinateTicks` to 32 (`engine::kiTickRate`). A spawn
  gets no arrival values: `transferLockTimer` and `arrivalGracePeriod` stay 0
  unless the entry sets them. `bTransfer` defaults to false, so the
  collection's `Spawn` starts the row as a fresh spawn; an omni then takes full
  armor and shield (`kfOmniArmor`, `kfOmniShield`) whatever `fArmor` and
  `fShield` say, so an entry that sets `bTransfer:true` also sets both. The
  server sets the rest: a minted global id for `SpawnOmni` and an empty client
  GUID, so an injected omni is synthetic AI and not an ownership harness rig; a
  missile starts with no homing target, which only in-tick code sets. A
  `SpawnExplosion` starts at the simulation time of the tick it is applied
  after, in its `coord`.
  An `UpdateOmni` entry's `bUseMissiles` takes effect `uiPendingWeaponModeTicks`
  ticks after the tick it enters; `0` leaves it pending.

  The batch also fails during replay playback, on an inactive `coord`, on an
  unknown key, on a value outside its member type's form above, on a
  non-finite value, on a `vecPosition` outside the cell, on a `SpawnBlaster` or
  `SpawnExplosion` `iTypeIndex` that is not an adoptable blaster or explosion
  type, on a `SpawnExplosion` `iTrailCount` below 0 or
  `fTimePercent` outside `[0,1]`, on a `SpawnOmni` `vecVelocity` whose XY length
  exceeds `kfOmniMaximumSpeed` (33.33 m/s), on a `navigationDelay` or
  `navigationDelaySeconds` outside `[0,60]`, and on a `nextBlasterFireTime`
  outside `[0,kBlasterFireInterval]`. The command makes no terrain check; a
  blaster destroys itself on terrain contact, so a `SpawnBlaster` that must
  survive needs a terrain-clear `vecPosition` (the probe in
  [replay.md](replay.md#replay-transfer-capture-harness-rig)). It also puts no
  bound on how many entries one cell's tick receives: every due entry of a
  coordinate joins that tick's one published batch. A batch over 1,024 status
  changes per cell tick (`engine::kiMaximumStatusChangesPerCell`) is still
  published and applied whole, but the server and every subscribed client log a
  warning and hit a debug break as their status-change buffers grow past that
  size, so keep one cell's entries for one tick at or below 1,024. The one hard
  limit is 65,535 entries of a single status-change type in one cell's tick,
  which the server's batch encoder asserts.

  Edge coast: to send an omni out across a cell's east edge, queue
  `{"coord":<cell>,"type":"SpawnOmni","vecPosition":[433.86,0],"vecVelocity":[33.33,0],"transferLockTimer":1}`:
  433.86 m is half the cell width (`cell_coordinate_probe` `area.width`) less
  15.5 ticks of travel at `kfOmniMaximumSpeed`, so the omni coasts under its
  transfer lock, which skips navigation and acceleration, and crosses into the
  eastern neighbor about 16 ticks after the tick it is applied, through the
  ordinary transfer producers; that crossing creates the neighbor when it is
  not active. A smaller margin crosses sooner: `vecPosition:[449.5,0]` crosses
  on the first tick after the one it is applied. For another edge, mirror or
  swap the components. Terrain near that path can push the coast off its line; then pick
  another cell.

  `tick` is an absolute `status.tick` value and must be a JSON integer greater
  than the current `status.tick`; omitted, it means the next tick,
  `status.tick + 1`. A spawn entry is applied right after exactly its target
  tick, with that tick's arrivals, so its row is in the frame published for
  that tick and first simulated on the next one; a destroy or update entry
  enters the input of exactly its target tick. Both hold at any timescale and
  in catch-up bursts that run several ticks per update; a pause stops the
  clock, so entries wait. An entry whose coordinate is inactive or whose frame
  is not ready at its target tick stays queued and applies with the first later
  tick that can take it. An active recording captures released entries as it
  captures natural ones: spawns in their tick's post-dispatch channel, destroy
  and update entries in their tick's input. Load, `reset`, and session
  replacement clear both queues, and so do a replay playback start and abort,
  which replace state as `load` does. Recording start, stop, failure, and
  cancellation and a replay transient-state clear drop no queued entry.

  `pauseAfterWriterInput` requires `kbDebugInput` and must be a JSON boolean
  whenever present; the response echoes it. `false` imposes nothing further and
  arms nothing. `true` also requires a spawn entry in the batch, active
  recording or a paused, unconsumed `replay_record {"start":true}`, no automatic
  pause already armed, and no entry carrying `tick`. After queueing, `true` arms
  count `1` for a pending start, or snapshots the active writer-input count `N`
  and pauses at `N + 1`, the writer input of the event tick whose post-dispatch
  record is written later in the same update.

  The response is `{"injected":int,"globalIds":[...],"deferred":bool}`. `globalIds` lists the ids minted for `SpawnOmni` entries in entry order, and `deferred` is true while the server is paused. A request carrying `pauseAfterWriterInput` adds its echo.

Every grid-coordinate parameter of this project's server commands (`coord`, `fleetWantedCoordinate`) follows the engine
[grid-coordinate rule](../../../../.agents/skills/agent-harness/references/command-reference.md#server-commands),
and a bad one fails `inject_payload` before that entry mints an ID and before anything is queued.

`inject_payload` fails during replay playback. While paused, it remains accepted and queued; `deferred:true` means each entry applies on the next unpaused tick, or on its `tick` when that is later. A speculative client reconciliation line after that tick is not a confirmed desync; only `CONFIRMED DESYNC after full rollback/replay` proves one.
