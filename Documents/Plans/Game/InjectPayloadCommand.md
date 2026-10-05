<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-05T23:31:12.685Z","dependsOn":[]} -->
# One generic `inject_payload` server command with exact per-tick scheduling; retire the four fixture commands

Line numbers cite baseline `63669fbb61d2f1dbc0b16ae616c7ea661eb57a87`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

`Documents/Investigations/AgentFrameInjection.md` found that each runtime test needing a specific server frame state grew its own server command: `spawn_players` (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:675-715`), `inject_status_changes` (`:516-673`), `replay_transfer_fixture` (`:256-417`), and `inject_outward_transfer` (`:720-815`). All four end in one of two existing queues: `QueueAgentStatusChange` (`:872-877`) or `QueueReplayTransferFixture` (`:879-902`). Both queues release every entry on their next drain; nothing can place an entry on a chosen simulation tick.

User decisions:

- Option A: one generic command, `inject_payload`.
- Preconditions live in a per-kind validation table inside the new command.
- All four old commands are retired in this change, with no compatibility alias.
- An out-of-cell `SpawnPlayer` spawn position is rejected at the command. Today the consumer skips it silently (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:261-264`) and the command still replies `ok`.
- The server-side Blaster terrain-clear search is deleted: the blaster branch of `CommandReplayTransferFixture` (`ServerSimulationFixtures.cpp:345-387`), including its `BuildElevationGrid` materialization (`:353-359`). `TransferBlaster` `vecPosition` is caller-supplied like every other transfer kind, with the cell-centre default and no terrain search.
- The JSON field names are not hand-written. They come from a generated, checked-in header that a new deterministic PowerShell 7 script writes from the payload structs in `StatusChange.h`, and a static check fails when that header is stale. Each key is the C++ member name verbatim (`## Generated field names`).
- In the user's words, the command is "one for injecting Status Change events (immediately or at a specific Tick)": each entry applies either on the next tick, as today, or at a caller-chosen simulation tick (`## Tick scheduling`).
- "Exact at any speed": a scheduled status change enters exactly the input of its target tick even when one server update runs several ticks (timescale or a catch-up burst). The user accepted that this changes the server's per-tick input preparation and raises the Plan to Tier 3.

The author's recommendations, each with its rationale:

1. `TransferPlayer` `nextBlasterFireTimeSeconds` is refused when out of range, through the existing `PlayersPostRender::IsBlasterFireTimeInRange` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:198-201`). A finite out-of-range value would otherwise assert in simulation (`PlayersCombat.cpp:295`) and make a save or replay unreadable (`Players.h:215-218` `PostRead`).
2. `iPlayerUuid` is required for `UpdatePlayer` and `UpdateFleet`, as for `DestroyPlayer`. This keeps today's behavior (`ServerSimulationFixtures.cpp:419-426`, `:501`, `:507`).
3. The generator is a PowerShell script rather than DataPacker, because the stale check runs in the static pass, and Shared data mode, the default for code changes, disables every DataPacker step (`.agents/skills/compile/references/runtime-data-mode.md:7`).
4. The per-tick status-change release hooks into the existing per-tick preparation `ServerSession::PrepareTick` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:43-66`), which `GameBase::ServerUpdate` already calls once per tick (`Engine/Source/GameBase.cpp:368`), so `GameBase.cpp` itself does not change (`## Tick scheduling` `### Per-tick release`).

Facts the implementer can rely on (verified at the baseline):

- The clients-waiting-for-spawn refusal no longer exists. No `ServerSimulationFixtures.cpp` handler checks it.
- The `status` queue counts already exist (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:108-109`), and `status.tick` is `gpGame->miTickCounter` (`:83`).
- Agent JSON falls under the agent-channel exemption (`Engine/Source/Agent/AGENTS.md:18`), so validation blocks only what would assert, crash, or break the replay reader.
- No harness script, skill, or Plan calls the four commands. `git grep` finds only C++, harness docs, `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`, and three `Documents/` files.
- Alignment is `gpGame->mPlayerAlignment` (`Projects/BrokenEngineSandbox/Source/Game.h:158`); no `PlayerAlignment()` function exists.
- Every server log line carries the tick of the thread's `LogTickScope` (`Common/Log/Log.cpp:121-130`); the tick loop sets it per tick (`Engine/Source/GameBase.cpp:366`) and dispatch workers inherit it (`Common/Threading/Multithreading.h:67`).
- A server update that runs other than one tick logs `ServerUpdate FullTicks: {n} (expected 1)` at `kWarning` (`Engine/Source/GameBase.cpp:340-343`), stamped with the last tick finalized before that update (`:300`); that update then runs ticks stamp+1 to stamp+n. The accumulator caps a burst at 4 ticks (`Engine/Source/Frame/TimeStep.h:42`).
- An `UpdatePlayer` whose `iPlayerUuid` matches no player in its cell logs `ProcessUpdateStatusChanges::kUpdatePlayer Uuid: {uuid} NOT FOUND in idToIndexMap` at `kWarning` in the tick that consumes it (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:336-339`), and otherwise changes no state.
- The Network `Verbose` publication line is clamped away on the Debug server (`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:22`), so no acceptance row depends on it.

## Command shape

`inject_payload {"entries":[...], "navQueryActivation"?:{"arm":true}, "pauseAfterWriterInput"?:bool}`

- Top-level keys are exact (the idiom at `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:11-33`). `entries` is a non-empty array. The whole batch validates before any queue mutation, so it is all-or-nothing.
- Entry shape: `{"coord":[x,y],"type":<StatusChangeTypeName>,"tick"?:int, ...fields}`. `type` is parsed against `StatusChangeTypeName` (`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:38-53`). `DefaultDataForType` (`:163-173`) selects the variant, and `IsTransferType` (`:31-34`) selects the queue. Only `coord`, `type`, `tick`, and that kind's field names are accepted; any other key throws. `coord` is parsed by the existing `CoordinateFromParameter` (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.h:12`). No payload member is named `coord`, `type`, or `tick` (`StatusChange.h:55-153`), so the entry keys cannot collide. `tick` is specified under `## Tick scheduling`.
- Field keys are the payload member names verbatim, resolved through the generated header (`## Generated field names`). There is no rename rule.
- JSON value form by member type — the only member types the command parses:
  - `bool`: JSON boolean.
  - `int64_t`: JSON integer.
  - `uint8_t`: JSON integer in [0, 255].
  - `float`: JSON number, finite.
  - `std::chrono::duration<float>`: JSON number of seconds, finite.
  - `engine::GridCoord`: `[x,y]` through `CoordinateFromParameter(rEntry, <member name>)`.
  - `XMVECTOR`: `[x,y]` in local metres, finite. The command fills z (`engine::gBaseHeight.mfCurrent` for `vecPosition`, 0 otherwise) and W (1 for `vecPosition`, 0 otherwise). That rules out by construction the W-lane assert in `common::ValidateVector` (`Common/Math/MathUtils.h:33-48`) at the Spawn boundaries (`Players.cpp:392-394`, `Spaceships.cpp:528-530`, `Blasters.cpp:120-121`, `Missiles.cpp:381-384`).

  The parse is one function template over the member type; an instantiation for any other type fails the build through a dependent `static_assert`. Server-owned members of other types (`engine::ClientGuid`, `engine::AlignmentIdentifier`, `engine::GlobalId`, `uint16_t`, `uint64_t`) are never in a per-kind field list, so they are never instantiated.

### Per-kind table

Caller-settable fields, cross-checked against the wire subsets in `Engine/Source/Network/NetworkSerialization.cpp:12-120` and `:142-185`:

| Kind | Fields | Struct |
|---|---|---|
| `SpawnPlayer` | `fSpawnOffsetX`, `fSpawnOffsetY`, `bIsFlagship`, `fleetWantedCoordinate` | `SpawnPlayerData` (`StatusChange.h:55-67`) |
| `DestroyPlayer` | `iPlayerUuid` (required) | `DestroyPlayerData` (`:69-73`) |
| `UpdatePlayer` | `iPlayerUuid` (required), `bUseMissiles`, `navigationDelaySeconds` | `UpdatePlayerData` (`:75-82`) |
| `UpdateFleet` | `iPlayerUuid` (required), `fleetWantedCoordinate` (required), `bIsFlagship` | `UpdateFleetData` (`:84-91`) |
| `TransferPlayer` | `vecPosition`, `vecDirection`, `vecVelocity`, `fHealth`, `fShield`, `nextBlasterFireTimeSeconds`, `nextSecondarySpawnTimeSeconds`, `shieldCooldownSeconds`, `shieldDownSoundCooldownSeconds`, `animationTimeSeconds`, `navigationDelaySeconds`, `fleetWantedCoordinate` | `TransferData` (`:93-153`); wire `NetworkSerialization.cpp:46-65` |
| `TransferSpaceship` | `vecPosition`, `vecDirection`, `vecVelocity`, `fHealth`, `nextBlasterSpawnTimeSeconds`, `fDeltaRotation` | wire `:20-29` |
| `TransferBlaster` | `vecPosition`, `vecVelocity`, `uiTypeIndex` | wire `:12-18` |
| `TransferMissile` | `vecPosition`, `vecDirection`, `vecVelocity`, `fAcceleration`, `deltaRotationDelaySeconds`, `timeSeconds`, `nextJitterSeconds`, `fDeltaRotation`, `fDeltaRotationMaximum`, `fPitch` | wire `:31-44` |

In code, each kind's row is a hand-written list of pointers to members (for example `&TransferData::vecPosition`), each with its required flag. The list is the validation policy and stays hand-written; every key string comes from the generated `kStatusChangeFieldName<pMember>`, and the command `static_assert`s that name is non-empty, so a listed member the header lacks fails the build. An entry key that matches no member of its kind's list throws.

A missing required field throws. Every other field is optional and takes the default below.

Server-owned fields, absent from every list and set by the command after parsing:

- Global id: `gpGame->miNextGlobalId++` for `SpawnPlayer` (`iGlobalId`) and `TransferPlayer` (`globalPlayerId`), 0 otherwise.
- Alignment: `gpGame->mPlayerAlignment`.
- Client GUID: empty (`clientGuid`, `uiClientGuidHigh`, `uiClientGuidLow`).
- `uiPlayerFlags`: 0.
- Pending-tick fields at today's values: `kiTickRate` for `UpdatePlayer` and `UpdateFleet` (`ServerSimulationFixtures.cpp:501`, `:507`); 0 for spawn (`:464`) and for transfers (the struct default).

Defaults:

- Struct defaults apply, including `SpawnPlayer` `fSpawnOffsetX` 45 and `fSpawnOffsetY` -12 (`StatusChange.h:62-63`) and `navigationDelaySeconds` 60.
- `fleetWantedCoordinate` defaults to `coord`.
- Transfer defaults are copied from today's replay fixture (`ServerSimulationFixtures.cpp:344`, `:389-401`): cell-centre `vecPosition`, `vecDirection` and `vecVelocity` `[1,0]`, `fHealth` and `fShield` 1, blaster `uiTypeIndex` `PlayersInterpolate::suiBlasterTypeIndex`, and missile `fDeltaRotationMaximum` 2.
- `TransferBlaster` takes the same cell-centre `vecPosition` default as every other transfer kind. There is no server-side terrain search; a caller that needs the blaster to survive terrain contact supplies a terrain-clear `vecPosition`.

### Refusals (validation table)

- Every kind: replay playback (`gpGame->mbReplaying`, as at `ServerSimulationFixtures.cpp:569-572`).
- Status-change kinds: `coord` must be active (today's per-entry check, `:443-447`).
- Transfer kinds:
  - The `kbDebugInput` build is required.
  - A non-player destination must be live (`IsDestinationLive`, today inside `QueueReplayTransferFixture` at `:894-897`). This check moves into the validation pass, so the batch stays atomic.
- Every position — `SpawnPlayer` (`fSpawnOffsetX`, `fSpawnOffsetY`) and every transfer `vecPosition` — must be finite and inside `LocalFrameArea()` (`Engine/Source/Frame/FrameUtils.h:121-124`):
  - A transfer player otherwise asserts (`Players.cpp:387`).
  - An out-of-cell `SpawnPlayer` is rejected instead of being skipped silently.
- `TransferPlayer` `vecVelocity` must have an XY length of at most `kfPlayerMaximumSpeed` (`Players.h:34`). An arriving player coasts under its transfer lock without navigation or acceleration (`Players.cpp:739-743`); a faster injected velocity can carry it across the destination cell within one tick, so the next transfer computes a delta of at most one cell (`Engine/Source/Frame/FrameUtils.h:160-161`), converts the position by that one cell width (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:755`), and the arrival lands outside its new cell and hits the in-cell `ASSERT` (`Players.cpp:387`). The bound is the speed navigation itself never exceeds.
- Every float must be finite (`Spaceships.h:115-124` `PostRead`).
- `navigationDelaySeconds` must pass the existing `PlayersPostRender::IsNavigationDelayInRange` (`Players.h:204-207`).
- `uiTypeIndex` must pass `IsAdoptableStatusChange` (`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:109-117`).
- `nextBlasterFireTimeSeconds` must pass the existing `PlayersPostRender::IsBlasterFireTimeInRange` (`Players.h:198-201`).
- `tick`, when present, must satisfy `## Tick scheduling` `### Validation`.
- `navQueryActivation` keeps today's gates and transaction (`ServerSimulationFixtures.cpp:538-566`, `:573-607`, `:619-661`): `kbProfiling`, a normal unpaused 1/1 server, recording and replay inactive, and a free event slot. It is additionally rejected when the batch holds a transfer entry, because its prepared-queue swap (`:627-660`) covers only the status queue, and when any entry carries `tick` (`### Interactions`). Status changes and the arm still commit as one transaction.
- `pauseAfterWriterInput`, split by when each condition applies (today only `true` is gated, `:282-294`):
  - When the key is present, with either value: it must be a JSON boolean, and the build must be `kbDebugInput`. The response echoes it.
  - Only when it is `true`:
    - the batch holds a transfer entry;
    - recording is active or a paused pending start exists (`:268-273`);
    - no automatic pause is already armed (`:291-294`);
    - no entry carries `tick` (`### Interactions`).

  `false` imposes nothing further and arms nothing. `true` is armed after queueing, as today (`:407-410`).
- `source` is dropped. It never entered the payload, and `PrepareReplayTransfers` is keyed only by destination (`:956-959`).

Build gate: status-change kinds stay available on non-debug builds, because Profile needs `navQueryActivation` with `kbDebugInput=false`. Transfer kinds and `pauseAfterWriterInput` require `kbDebugInput`, as the transfer queue already does (`:881-884`).

Response: `{"injected":n,"globalIds":[...],"deferred":bool}`.

- `globalIds` lists the ids of `SpawnPlayer` and `TransferPlayer` entries, in entry order.
- `deferred` reports the paused flag, as today, whether or not entries carry `tick`.
- The response also carries the `navQueryActivation` object when the arm was requested (`:649-654`) and the `pauseAfterWriterInput` echo when that key was sent.

## Tick scheduling

### How each tick's inputs reach dispatch, replay, and clients today

One server update (`Engine/Source/GameBase.cpp:298-453`) runs in this order:

1. Agent commands drain (`:312-315`). `status.tick` here equals `miTickCounter`, the last finalized tick.
2. `mfLastDeltaTime` is set to the update's tick count times the tick length (`:350`), then `PrepareActiveSet` (`:352`, `:944-960`) calls `Game::BuildFrameInputs` (`:959`, `Projects/BrokenEngineSandbox/Source/Game.cpp:295-298`), which is `ServerBroadcaster::BuildFrameInputs` (`Engine/Source/Network/Server/ServerBroadcaster.cpp:18-82`). On an advancing update it injects the natural changes (player updates, fleet timers and flagship updates, waiting-client spawns; `:51-63`), then `DrainPendingAgentStatusChanges` (`:71`) moves queued agent changes into `mFrameInputs`, and the snapshot loop (`:73-80`) copies every non-empty `statusChanges` into `mBroadcastStatusChanges`. All of this runs once per update.
3. Each loop iteration (`GameBase.cpp:359-422`) increments `miTickCounter` to `T` (`:363`), sets the per-tick log scope (`:366`), and calls `ServerSession::PrepareTick` (`:368`; `ServerSession.cpp:43-66`), which during normal play recomputes the active set and adds an empty `FrameInput` for each newly active coordinate (`:56-65`). Then `SyncReplayTick` (`GameBase.cpp:378-380`) lets each active replay writer record that coordinate's `FrameInput` for tick `T` (`Engine/Source/File/Replay.cpp:964-965`). Dispatch for `T` reads `mFrameInputs` (`GameBase.cpp:406`, `:480`).
4. `FinalizeFrameTick` (`:413`, `:511-531`): `HarvestTransfers` (`:516`; `Engine/Source/Network/Server/ServerTransferManager.cpp:293-318`) collects natural transfers, calls `DrainReplayTransferFixtures` (`:303`), sorts, hands each destination's batch to replay capture (`:309-315`), and applies it to the tick-`T` frame (`:317`). `SwapFrames` (`GameBase.cpp:523`) follows, then `CompleteTick(T)` (`:525`; `Engine/Source/Network/Server/ServerSessionRuntime.cpp:232-246`) builds tick `T`'s client publication from `mBroadcastStatusChanges` and the transfer manager's `mTransfers` (`ServerBroadcaster.cpp:84-216`), then clears `mTransfers` (`:215`) and `mBroadcastStatusChanges` (`ServerSessionRuntime.cpp:243`). Last, the loop clears every input's `statusChanges` (`GameBase.cpp:527-530`).

So a later tick (k ≥ 2) of a multi-tick update reaches clients with only what tick k itself produced: its `FrameInput` holds no status changes, because step 2 ran once and step 4 cleared both the inputs and the broadcast snapshot after tick 1; its publication carries only the transfers harvested at tick k. Server dispatch, replay capture (step 3, per tick), and the client publication (step 4, per tick) therefore agree tick by tick, and the frame CRC each client checks against matches. The one consequence is the gap the user decision closes: today an agent status change can enter only the first tick of an update.

### Per-tick release

The release of agent status changes, with the broadcast snapshot that follows it, moves from the once-per-update `BuildFrameInputs` to the per-tick preparation:

- New `ServerBroadcaster::PrepareTickStatusChanges()` (declared in `Engine/Source/Network/Server/ServerBroadcaster.h`, defined in `ServerBroadcaster.cpp`) holds what `BuildFrameInputs` did at `:65-80`, unchanged in order: under `ScopedSuppressAllocationTracking` (as at `:20-21`), call `game::DrainPendingAgentStatusChanges(*game::gpServerSession)`, then run the snapshot loop that copies every non-empty `mFrameInputs` `statusChanges` into `mBroadcastStatusChanges`.
- `BuildFrameInputs` loses those lines; everything else in it stays, including the clear of `mBroadcastStatusChanges` (`:24`) and the natural-change injection (`:31-63`).
- `ServerSession::PrepareTick` calls `mpBroadcaster->PrepareTickStatusChanges()` after its per-coordinate `FrameInput` loop (`ServerSession.cpp:59-65`). `PrepareTick` already returns early during replay playback (`:46-49`), where `SyncReplayTick` fills both `mFrameInputs` and `mBroadcastStatusChanges` from the recording (`Replay.cpp:1047-1087`).

Why this is equivalent on every tick that has no scheduled entry:

- First tick of an update: between `BuildFrameInputs` and the first `PrepareTick`, nothing writes a `statusChanges` vector or `mBroadcastStatusChanges` (`GameBase.cpp:352-368`; `ServerSession.cpp:56-65` only adds empty inputs). The natural changes are already in `mFrameInputs` when the drain appends and sorts (`ServerSimulationFixtures.cpp:939-944`), so the input and the snapshot are the bytes today produces. The waiting-client spawns still precede the drain, as the comment at `ServerBroadcaster.cpp:61-62` requires.
- Later ticks: inputs and snapshot start empty (cleared at `GameBase.cpp:527-530` and `ServerSessionRuntime.cpp:243`), so with no released entry the snapshot loop copies nothing, as today.
- Zero-tick updates never reach `PrepareTick` (the loop body does not run), matching today's whole-map hold for `mfLastDeltaTime <= 0` (`ServerSimulationFixtures.cpp:910-913`).

Because every released change sits in tick `T`'s `FrameInput` before `SyncReplayTick` and dispatch, and in tick `T`'s `mBroadcastStatusChanges` before `CompleteTick(T)`, dispatch, replay capture, and client publication stay consistent per tick by the same mechanism that serves natural changes today. No wire, replay, save, or CRC format changes; only the tick an injected change enters is new.

Transfers need no new hook: `DrainReplayTransferFixtures` already runs in the harvest of every finalized normal tick (`GameBase.cpp:413`, `:516`; `ServerTransferManager.cpp:303`), with capture and publication on that same tick (`ServerTransferManager.cpp:309-317`; `ServerBroadcaster.cpp:86`, `:182-188`). Releasing an entry there once its target is reached (`iTick <= miTickCounter`, `### Release rules`) already gives exact placement at any speed, so that call site stays.

### Target tick per entry

Each queued entry carries a target tick `iTick`:

- `tick` present: `iTick` is that value.
- `tick` omitted: `iTick = gpGame->miTickCounter + 1` at command time. The next tick to run is exactly that tick, so this reproduces today's next-tick behavior with one release rule and no sentinel value.

The author recommends one file-local struct in `ServerSimulationFixtures.cpp`, `struct ScheduledStatusChange { int64_t iTick = 0; StatusChange change; };`, as the element type of both maps in `ServerSimulationFixtureState` (`:22-26`), because a per-entry pair is the smallest change that lets the existing per-coordinate maps, counts, resets, and the `navQueryActivation` prepared copy keep their shape.

### Validation

`tick` must be a JSON integer greater than `gpGame->miTickCounter` (that is, greater than `status.tick` read in the same drain). A smaller or equal value names a tick that is already finalized and is rejected with the batch. The earliest accepted value, `miTickCounter + 1`, is the next tick to run, so `tick:status.tick+1` behaves exactly like an omitted `tick`. The author recommends no upper bound: a far-future entry only waits in the queue, and load and reset clear it (`### Clearing`).

### Release rules

Both drains use one rule: release the entries with `iTick <= gpGame->miTickCounter`, where `miTickCounter` is the tick being prepared or finalized.

- `DrainPendingAgentStatusChanges` (`ServerSimulationFixtures.cpp:904-947`) runs from `PrepareTickStatusChanges`, after the loop has advanced `miTickCounter` to `T` (`GameBase.cpp:363`). Its two whole-map holds (`mfLastDeltaTime <= 0` and replay playback, `:910-917`) are deleted: its only caller now runs only inside a dispatching tick (`GameBase.cpp:359`, with `mfLastDeltaTime > 0` from `:350`) and never during playback (`ServerSession.cpp:46-49`). It keeps the per-coordinate holds (inactive coordinate, missing frame, null `pCurrent`; `:922-938`). For a coordinate that passes them it appends the released entries' `change` to that coordinate's `statusChanges`, keeps the stable type sort (`:939-944`), keeps the remaining entries in their original order, and erases the coordinate's map entry only when nothing remains.
- `DrainReplayTransferFixtures` (`:949-961`) runs inside the harvest of tick `T == gpGame->miTickCounter`. For each coordinate it calls `PrepareReplayTransfers` only when at least one released entry survives the liveness re-check below, passing exactly those entries, and keeps the unreleased entries. A coordinate holding only future-tick entries, or whose released entries were all dropped, gets no call, because `PrepareReplayTransfers` creates the destination frame and an `mTransfers` entry even for an empty span (`Engine/Source/Network/Server/ServerTransferManager.cpp:320-332`), which would create a scheduled `TransferPlayer`'s destination before its target tick. Before passing a released non-player entry, it re-checks `rTransferManager.IsDestinationLive(coordinate)` and drops the entry with a `LOG(kNetwork, kWarning, ...)` line naming the coordinate and type when the destination is no longer live. This mirrors the drop `CollectTransfers` applies to organic transfers (`ServerTransferManager.cpp:82-86`); without it a scheduled entry whose destination stopped being live before its tick would reach the `DEBUG_BREAK` in `ApplyPreparedTransfers` (`:137-141`).
- Placement is exact at any simulation speed: every tick, first or later in its update, prepares its own status-change release and finalizes its own transfer release. A pause stops the clock, so it delays no target.
- An entry is late only when its coordinate is held at its target tick — an inactive coordinate, or a frame not ready (status changes); recording and playback do not hold either queue on a normal tick. A late entry applies at the first tick at or after its target that can take it; it is never dropped for being late, except a transfer entry dropped at release by the liveness re-check. An omitted `tick` keeps today's next-tick behavior, including these holds.

### Interactions

- `navQueryActivation` with any `tick` entry is rejected. The arm's floor is `queuedAtTick + kiTickRate + 1` (`:642-648`), which assumes the batch enters the next tick; a later target would let the arm sample a tick the batch has not yet reached. Rejecting the combination keeps the arm's contract unchanged.
- `pauseAfterWriterInput:true` with any `tick` entry is rejected. The arm pauses after the next writer input (`ReplayFixtures::ArmPauseAfterNextWriterInput`, `:407-410`), which is the transfer's event tick only when the transfer is harvested on the next tick.
- Replay recording records released status changes inside the tick's `FrameInput` and released transfers in the post-dispatch channel, the same as unscheduled entries; nothing new reaches the replay format.

### Counts

`pendingAgentStatusChangeCount` and `pendingTransferFixtureCount` (`AgentCommandsServer.cpp:108-109`) keep counting every queued entry, scheduled ones included, until it is released (or, for a transfer, dropped at release). `CountPendingAgentStatusChanges` and `CountReplayTransferFixtures` (`ServerSimulationFixtures.cpp:979-1007`) need no change beyond compiling against the new element type.

### Clearing

Existing clears cover scheduled entries with no new code:

- Load, reset, and replay state replacement run `ServerSession::ResetClientsForLoad` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:372-411`), whose `mpTransferManager->ResetState()` (`:404`) and `mpBroadcaster->ResetState()` (`:406`) clear both queues (`ServerTransferManager.cpp:368-373`, `ServerBroadcaster.cpp:277-282`). A load can move `miTickCounter` backwards (`Engine/Source/File/GridSave.cpp:192`), so clearing there also keeps a stale target from waiting on a rewound clock.
- `game::OnReplayStreamsInvalidated` clears the whole transfer queue, future-tick entries included, and leaves the status queue (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:41-44`). It runs on recording stop (`Engine/Source/File/Replay.cpp:818`), on each recording-start failure (`:760-798`), on replay transient clears (`:277`), and at `:304`. Cancelling a not-yet-started recording also clears the transfer queue (`ServerSimulationFixtures.cpp:84-88`).
- Session replacement or destruction clears the whole fixture state (`Bind`, `:31-38`; `DetachServerSimulationFixtures`, `:1009-1015`, called from `ServerSession.cpp:38`).

## Generated field names

### Generator script

`.agents/scripts/Write-AgentFieldNames.ps1`, PowerShell 7, beside the repository's other source-reading script `.agents/scripts/Test-CollectionLayout.ps1` (`.agents/references/collection-layout-auditor.md:3`). PowerShell is the default script language (`.agents/skills/external-skill-creator/references/authoring.md:36`).

- Invocation, from the worktree root: `pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1` writes the header; adding `-Check` regenerates in memory, writes nothing, and compares. `-Check` is the only parameter. Both paths are resolved from `$PSScriptRoot`, so no input-path parameter exists.
- Input: `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`. The payload structs are the alternatives of `using StatusChangeData = std::variant<...>;` (`StatusChange.h:155-161`), in that order: `SpawnPlayerData`, `TransferData`, `DestroyPlayerData`, `UpdatePlayerData`, `UpdateFleetData`. Every `//` comment is stripped before parsing.
- Inside each variant struct's braces (found by brace depth from its `struct NAME` line), the generator emits every line matching the data-member form `TYPE NAME = INIT;` or `TYPE NAME {};`, where TYPE may contain `::` and `<...>`, and ignores every other line. Every data member in the five structs (`StatusChange.h:55-153`) has one of these two forms. A name starting with `operator` is never a data member, so the defaulted `bool operator==(...) const = default;` lines (`:66`, `:72`, `:81`, `:90`) are ignored.
- Fails loudly — exit 2, one message naming what is missing, and nothing written — only on a missing variant declaration or on a variant alternative with no struct definition.
- The generator copies member names only and never maps a type: the value type reaches C++ through the pointer to member, and the command's parse rejects an unsupported type at compile time (`## Command shape`). It emits every data member, server-owned ones included, because which members a kind may set is the hand-written per-kind policy, not a property of the struct.
- Output is deterministic: it depends only on the input text, in variant then declaration order, with no timestamp, absolute path, or hash-ordered collection. It is written as UTF-8 without a BOM, LF line endings, one trailing newline — the same bytes as the neighbouring `ServerSimulationFixtures.h` and the repository's `* text=auto eol=lf` checkout (`.gitattributes`).
- Exit codes: 0 written or up to date; 1 under `-Check` when the header is missing or its bytes differ, with a message naming the regenerate command; 2 a parse failure. This matches the 0/1/other mapping the runner already applies to `Validate-Skill.ps1` (`.agents/scripts/Invoke-StaticChecks.ps1:292-293`).

### Generated header

`Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentFieldNames.h`, beside its only consumer, `ServerSimulationFixtures.cpp`, which is a server-only file (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj:506`). Shape:

- A first-line comment naming the generator script and `Frame/StatusChange.h`, with the instruction to regenerate rather than edit.
- `#pragma once` outside a whole-file `#if defined(BT_SERVER)` wrap (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:34`).
- In `namespace game`: the primary `template <auto pMember> inline constexpr std::string_view kStatusChangeFieldName {};`, then one explicit specialization per data member, grouped per struct under a `// <StructName>` line, of the form `template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::vecPosition> = "vecPosition";`.
- No `#include`: `StatusChange.h` comes in through the game PCH (`StatusChange.h:3`).

The header is produced only by running the script, never by hand. It is a server `ClInclude` in `BrokenEngineSandboxServer.vcxproj` and its filters file under `Game\Agent\Commands`, listed by its Source path like any other game header, reconciled through `/update-vcxproj`; the client project does not list it. Because the project rule at `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:36` currently says generated headers are `$(GameDataDirectory)` items, that line is reworded (`## In scope`).

### Static check

`.agents/scripts/Invoke-StaticChecks.ps1` gains one row, `agent-field-names`, added the way the existing rows are (`:254-322`, `:340-343`):

- Trigger: any inventory entry whose `path` or `oldPath` is `StatusChange.h`, the generated header, or the generator script. Deleted entries count, because a deleted header must fail; this is the same both-sides scan as `Invoke-ValidateSkillCheck` (`:262-268`) rather than `Get-ChangedPath`, which drops deletions (`:141`). When nothing triggers but the inventory is truncated, the row is `blocked`, as for `markdown-links` (`:309-311`).
- Run: `Write-AgentFieldNames.ps1 -Check` through `Invoke-StaticCheckProcess`, as `Validate-Skill.ps1` is run (`:291`). Exit 0 is `pass`, 1 is `fail`, any other is `blocked`; a non-passing row carries the script's output lines in `detail`.
- Like `validate-skill`, the row checks the working tree's copy even under `-Head`; the header comment's statement of which checks read the commit (`:14-15`) is extended to say so.
- The script joins the composed-script presence check (`:329-333`). The header comment's list of checks (`:1-4`) and the "Both rows are always present" comment (`:339`) are updated to three rows.
- `schemaVersion` stays `broken-engine-static-checks/v2`: the row shape (`:220-222`) is unchanged, and no tracked file parses the version or a row count (`git grep broken-engine-static-checks` finds only the runner).

## In scope

- `.agents/scripts/Write-AgentFieldNames.ps1` (new): the generator in `## Generated field names` `### Generator script`, with a header comment documenting its invocation, `-Check`, the data-member line forms it emits, and the exit codes.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentFieldNames.h` (new): generated by running the script; never hand-edited.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` and `.vcxproj.filters`: one `ClInclude` for the generated header under `Game\Agent\Commands`, via `/update-vcxproj`.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:36`: reword the generated-header sentence so it covers both kinds — DataPacker-generated headers stay property-based `$(GameDataDirectory)\*.h` items, while a script-generated header checked in under `Source/` is listed by its Source path with the affinity of its consumer, like any other header.
- `.agents/scripts/Invoke-StaticChecks.ps1`: the `agent-field-names` row per `### Static check` — one new check function, its call in the `$checks` array (`:340-343`), the composed-script presence check (`:329`), and the header comments at `:1-4`, `:14-15`, and `:339`.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp` and `ServerBroadcaster.h`, per `## Tick scheduling` `### Per-tick release`:
  - Add `PrepareTickStatusChanges()`, holding the agent drain and the broadcast snapshot loop moved from `BuildFrameInputs` (`:65-80`), under its own `ScopedSuppressAllocationTracking`.
  - Remove those lines from `BuildFrameInputs`; reword the comment at `:61-62` so it names the per-tick drain, and rewrite the drain comment (`:65-70`) for its new place: the drain runs once per dispatched tick and holds only per coordinate.
  - Declare the method in the header and update the header comment (`ServerBroadcaster.h:5-9`), which says the fixture queue drains "at the existing pre-snapshot phase", to say it drains once per tick before that tick's snapshot.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp` `PrepareTick` (`:43-66`): call `mpBroadcaster->PrepareTickStatusChanges()` after the `FrameInput` loop (`:59-65`).
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp`:
  - Add `CommandInjectPayload` (parse, per-kind table, `tick` validation, validation pass, then queue), including `#include "Agent/Commands/AgentFieldNames.h"` and the one member-type parse template in `## Command shape`.
  - Reuse `CoordinateFromParameter`, `IsCoordinateActive`, `IsNavigationDelayInRange`, `IsBlasterFireTimeInRange`, `kfPlayerMaximumSpeed`, the `navQueryActivation` parse and transaction block (`:538-566`, `:573-607`, `:619-661`; its prepared copy at `:627-631` takes the new element type), `QueueAgentStatusChange`, and `QueueReplayTransferFixture`.
  - `ServerSimulationFixtureState` (`:22-26`): add `ScheduledStatusChange` and make it the element type of `pendingAgentStatusChanges` and `replayTransferFixtures` (`## Tick scheduling` `### Target tick per entry`).
  - `QueueAgentStatusChange` (`:872-877`) and `QueueReplayTransferFixture` (`:879-902`): take the entry's target tick and store it with the change; the liveness check moves out of `QueueReplayTransferFixture` into the command's validation pass.
  - `DrainPendingAgentStatusChanges` (`:904-947`) and `DrainReplayTransferFixtures` (`:949-961`): the release rules in `### Release rules`, including deleting the status drain's two whole-map holds (`:910-917`) and the release-time liveness drop for non-player transfers.
  - Delete `CommandReplayTransferFixture` (`:256-417`) with its blaster terrain-clear search and `BuildElevationGrid` materialization (`:345-387`), which `CommandInjectPayload` does not carry over; `PlayerUuidFromParameter` and `NavigationDelayFromParameter` (`:419-437`) where no longer used, `BuildInjectedChange` (`:439-514`), `CommandInjectStatusChanges` (`:516-673`), `CommandSpawnPlayers` (`:675-715`), `CommandInjectOutwardTransfer` (`:717-815`), and `AreAdjacent` (`:44-49`) if unused.
  - In `ExecuteServerSimulationFixtureCommand`, replace the four names with `inject_payload` in the name gate (`:819`) and in dispatch (`:849-868`).
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.h`: the `QueueAgentStatusChange` and `QueueReplayTransferFixture` declarations (`:28-29`) gain the `int64_t` target-tick parameter.
- `Engine/Source/Network/Server/AGENTS.md:35`: the sentence "The broadcaster drains injected changes before its tick snapshot" states that the drain and the snapshot run once per dispatched tick, so an injected change can enter any tick of a multi-tick update.
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md:28-29`: remove the agent status-change drain and broadcast from the list of work done once "Within `BuildFrameInputs`"; state that they run in every tick's `PrepareTick`, after the once-per-update natural changes, so spawn construction still precedes the drain; keep line 29's holding rule, naming the per-coordinate holds.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`:
  - One `inject_payload` row replaces rows `:40`, `:48`, `:49`, and `:50`. It states that field keys are the payload member names, and documents `tick`: an absolute `status.tick` value that must exceed the current `status.tick`, omitted meaning the next tick. It states that a status change is in the input of exactly its target tick and a transfer is harvested at exactly its target tick, at any timescale and in catch-up bursts, and the late rule for a held coordinate. It also documents the `pauseAfterWriterInput` present-versus-`true` conditions, the `TransferPlayer` `vecVelocity` speed bound, and the rejection of `tick` with `navQueryActivation` or `pauseAfterWriterInput:true`. And it states that recording stop, a recording-start failure, and a replay transient clear drop every queued transfer entry, future-tick ones included, while queued status entries survive (`### Clearing`).
  - Update the `status` count wording at `:7` so both counts name `inject_payload` and include scheduled entries until release.
  - Remove `source` and `destination` from the coordinate-parameter list at `:52`, and rename `fleetWantedCoord` there to `fleetWantedCoordinate`.
  - Update the injection paragraph at `:54`.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md`: rewrite to `inject_payload` at `:10`, the `### Replay transfer-capture fixture` section (`:20-38`), `:46`, `:69`, and `:75`. In scenario D (`:35`) the `TransferBlaster` entry carries an explicit `vecPosition` into `[1,0]`. Before scenario D, the caller confirms that position terrain-clear in a separate non-recording probe: with playback cancelled first (`replay_play` again, then poll `status.replaying:false`, as scenario E does at `:36`), send `reset`, then a `TransferPlayer` into `[1,0]` so the destination is live, then, once that is harvested, a `TransferBlaster` at the same `vecPosition`; pause once `status.tick` ≥ the blaster's harvest tick + 1, then find a `query_collection {"coord":[1,0],"collection":"blasters"}` row whose `local` x and y equal `vecPosition + vecVelocity × (status.tick − harvest tick) / 32` within 0.05 m. The caller then sends `reset` before scenario D, so no probe player or blaster is present when D records. The command no longer searches for such a position.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md`: rewrite `:9`, `:15`, and `:24`.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md`: rewrite `:9`.
- Every recipe above uses the verbatim member-name keys of `### Per-kind table` (for example `iPlayerUuid`, `bUseMissiles`, `bIsFlagship`, `fleetWantedCoordinate`, `fSpawnOffsetX`, `vecPosition`, `vecVelocity`, `fShield`).
- Outward-transfer recipe: an explicit `TransferPlayer` with `vecPosition:[433.86,0]` and `vecVelocity:[33.33,0]`. That is half the 900 m cell width (`Engine/Source/Frame/GridCoord.h:44`) minus 15.5 × `kfPlayerMaximumSpeed` 33.33 (`Players.h:34`) / 32 Hz, and the speed is exactly the bound. Cell width comes from `cell_coordinate_probe` `area.width`.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`:
  - Update the command names and contracts at `:5` and `:11`; at `:11` also state that each queued entry carries a target tick and is released on exactly that tick, or on the first later tick its coordinate can take it.
  - At `:14`, delete the Blaster terrain-clear sentence ("a Blaster fixture must materialize ... terrain contact") and reword the rest of the bullet so it names the `inject_payload` transfer entries instead of replay transfer fixtures, keeping its queue-independence and determinism statements.
  - At `:15` (the bullet stating the game Agent owns command JSON fields), add that `inject_payload` field keys are the payload member names from the generated `AgentFieldNames.h`, regenerated with `pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1` after a payload struct in `Frame/StatusChange.h` changes, and that the static pass fails while it is stale.
- `Documents/Features/Engine/RawProfileActivationEventQueue.md:15`: rename the command.
- `Documents/Investigations/AgentHarnessFineGrainedControl.md`: rename the references that name a retired command as an existing mechanism (`:15`, `:62`, `:148`, `:309`, `:323`, `:364`, `:371`, `:399`). Lines that propose an option (`:180`) are reworded only so they name no retired command. Delete the sentence at `:157-158` that cites the blaster branch's terrain-clear search as an existing mechanism, since that search is removed.
- Delete `Documents/Investigations/AgentFrameInjection.md`, whose decision is now implemented. No tracked file links to it.

## Out of scope

- `Engine/Source/GameBase.cpp`: the update and tick loop, its `PrepareTick` call at `:368`, `PrepareActiveSet`, `FinalizeFrameTick`, and their order stay as they are; the per-tick hook is the existing `PrepareTick` call.
- `Engine/Source/Network/Server/ServerTransferManager.cpp`, `ServerSessionRuntime.cpp`, `Engine/Source/File/Replay.cpp`, and `Projects/BrokenEngineSandbox/Source/Game.cpp`: the transfer drain call site (`ServerTransferManager.cpp:303`), publication assembly and its clears, replay capture and playback, and the `BuildFrameInputs` forwarding stay as they are.
- In `ServerBroadcaster.cpp`, everything except the move in `## In scope`: `BuildTickPublication`, `ProcessUpdatePlayerRequests`, `ResetState`, and the natural-change injection in `BuildFrameInputs`, which keeps running once per advancing update.
- `Documents/Architecture/FrameUpdatePipeline.md`: its server loop stages (`:45-54`) are unchanged — the drain stays inside game tick preparation.
- In `ServerSimulationFixtures.cpp`: `ResetPendingAgentStatusChanges`, `ResetReplayTransferFixtures`, `CountPendingAgentStatusChanges`, `CountReplayTransferFixtures`, `DetachServerSimulationFixtures`, `CountCapturedReplayTransfers` (`:963-1030`) beyond compiling against the new element type; `Bind` (`:31-38`); the recording-cancel clear (`:84-88`); and the other replay commands (`:51-255`).
- The `status` counts in `AgentCommandsServer.cpp:108-109`, and any new `status` field, response field, or query exposing target ticks.
- `StatusChange.h`, `NetworkSerialization.cpp`, `FrameInput.cpp`, and every collection consumer.
- Every protocol, save, replay, or CRC version.
- `Documents/Investigations/MimallocPeakCommittedExceedsArenaReserve.md:13`, a historical run record.
- Any new query, geometry helper, harness script, skill, or alias for the retired names.
- DataPacker and every generated `$(GameDataDirectory)` header.
- Generator features beyond the `StatusChangeData` name table: no type mapping or type enum, no JSON writer, no equality or serialization generation, no struct outside the `StatusChangeData` variant (for example Frame collection structs), and no input- or output-path parameter. A later Plan that edits Frame fields directly (`Documents/Plans/Game/AgentFrameEdit.md`) extends the generator itself.
- The existing `validate-skill` and `markdown-links` rows, `Get-SessionChangeInventory.ps1`, the runner's `schemaVersion`, and every caller of the runner (`.agents/references/change-workflow.md:116`, `.agents/skills/sweep/SKILL.md:118`).
- The client project and its filters file.

## Execution card

### What does this plan do?

It replaces four special-purpose server test commands with one command, `inject_payload`. A harness caller uses it to queue any status change or cross-cell transfer by naming its kind and fields, where each field is named exactly as the C++ payload member. Each entry can also name the exact simulation tick it lands on, and it lands there at any game speed: the server now releases scheduled status changes on every tick it runs, not only on the first tick of each update. The field names come from a header that a new script generates from `StatusChange.h`, the static pass fails when that header is out of date, and the four old commands and their documentation are removed.

### Why this is good for the codebase

Each new runtime test that needed a particular frame state used to add its own command, with its own schema, refusals, and review round, and none could place an event on a chosen tick. Harness users and agents writing verification recipes now describe a new state as an entry object and can schedule several events at exact ticks in one request, even under fast timescales or catch-up bursts, instead of racing a pause. One validation table replaces four hand-built parsers, the JSON names cannot drift from the structs because they are generated and checked, and an out-of-cell spawn or an over-speed transfer becomes an explicit error rather than a silent no-op or a server assert.

- Goal: `inject_payload` with the per-kind field table, server-owned fields, defaults, refusals, build gate, and response shape in `## Command shape`; optional per-entry `tick` with the target-tick queues, the per-tick status-change release in `ServerSession::PrepareTick`, and the release rules in `## Tick scheduling`; its field names generated per `## Generated field names` with the `agent-field-names` static check. The four commands are deleted, and the docs are rewritten as listed under `## In scope`.
- Out of scope: everything under `## Out of scope`: `GameBase.cpp` and the tick loop's call order, the transfer drain call site, publication assembly, replay capture and playback, the natural changes' once-per-update cadence, resets and counts beyond the element type, the payload structs, wire, save, replay, and CRC formats, DataPacker, generator features beyond the `StatusChangeData` name table, the existing static-check rows and runner callers, the other replay commands, and compatibility aliases.
- Tier trigger: Tier 3 — the change moves where server tick inputs are assembled (determinism surface: which tick an input enters) and spans independently owned subsystems: engine Network Server (`ServerBroadcaster`), game Network Server (`ServerSession::PrepareTick`), game Agent, and the static-check runner. Evidence is under `## Classification evidence`.
- Interfaces and invariants:
  - Validation is all-or-nothing before any queue mutation.
  - A batch with `navQueryActivation` commits status changes and the arm as one transaction under `mCpuTimerMutex`, and is refused when it holds a transfer entry or any `tick`.
  - Transfer kinds and `pauseAfterWriterInput` exist only on `kbDebugInput` builds. `pauseAfterWriterInput` must be a Boolean whenever present; only `true` requires a transfer entry, active or paused-pending recording, no armed pause, and no `tick` entry.
  - Positions are finite and in-cell, the W lanes are set by the command, and a `TransferPlayer` `vecVelocity` XY length is at most `kfPlayerMaximumSpeed`.
  - Global id, alignment, client GUID, `uiPlayerFlags`, and pending ticks are server-owned.
  - Every transfer kind, `TransferBlaster` included, takes a caller-supplied `vecPosition` defaulting to cell centre; the command performs no terrain search and builds no elevation grid.
  - `iPlayerUuid` is required for `DestroyPlayer`, `UpdatePlayer`, and `UpdateFleet`; `nextBlasterFireTimeSeconds` is refused outside `IsBlasterFireTimeInRange`.
  - Every JSON field key is a payload member name taken from the generated `kStatusChangeFieldName`; no key string is hand-written in the command, and the per-kind member lists are the only hand-written policy.
  - The generated header is byte-identical to the script's output for the current `StatusChange.h`; the script's output depends on that text alone.
  - Every queued entry carries a target tick: `tick` when given (must exceed `status.tick`), else `status.tick + 1` at command time. Both drains release `iTick <= miTickCounter` for the tick being prepared or finalized, so an entry lands on exactly its target tick at any timescale and in catch-up bursts, unless its coordinate is held then, in which case it lands on the first later tick that can take it. A transfer is re-checked for destination liveness at release. An omitted `tick` behaves exactly as the next-tick behavior before this change.
  - On every tick, agent status changes are drained into that tick's `FrameInput` and copied into that tick's broadcast snapshot before replay capture, dispatch, and `CompleteTick`; with no scheduled entry, every tick's input and publication bytes equal the baseline's. Natural status changes keep their once-per-update cadence.
  - Load, reset, and session replacement clear scheduled entries through the existing clears; recording stop, a recording-start failure, and a replay transient clear drop every queued transfer entry, future-tick ones included, and keep status entries.
- Acceptance checks (via `/agent-harness`, Debug server unless noted):
  Each row names the check and the observation that settles it. "Step" means `pause {"paused":false}` immediately followed by `pause {"paused":true}`, then reading `status.tick` while paused; at timescale 1/32 (`timescale {"faster":false}` repeated, `commands-server.md:34`) one step advances zero or one tick.

  | # | Check | Expected observation |
  |---|---|---|
  | 1 | `inject_payload` with two `SpawnPlayer` entries on an active coord, no `tick` | two `globalIds`; `query_frame` players +2. On a paused server: `deferred:true`, and the spawn appears after resume |
  | 2 | `UpdatePlayer` `bUseMissiles`, `UpdateFleet`, and `DestroyPlayer` per the rewritten `verification.md` and `cross-cell.md` recipes | each round-trip is observed in `query_players`, `query_frame`, or `status` as the recipe states |
  | 3 | `TransferPlayer` at `vecPosition:[433.86,0]` with `vecVelocity:[33.33,0]` | the player leaves the cell about 16 ticks later and appears in the eastern neighbour |
  | 4 | `TransferBlaster` into a live destination at an explicit terrain-clear `vecPosition`, with `tick:T`; then step until paused at `status.tick >= T+1` | a `query_collection {"collection":"blasters"}` row on the destination whose `local` x and y equal `vecPosition + vecVelocity × (status.tick − T) / 32` within 0.05 m |
  | 5 | Rejections: unknown key (including a camel-case `pos`), `TransferMissile` with `fShield`, out-of-cell `vecPosition` or `fSpawnOffsetX`, non-finite value, `TransferPlayer` `vecVelocity:[40,0]` (XY length above `kfPlayerMaximumSpeed` 33.33), `TransferPlayer` `nextBlasterFireTimeSeconds` outside `IsBlasterFireTimeInRange`, `UpdatePlayer` or `UpdateFleet` without `iPlayerUuid`, transfer entry on a Profile build, `navQueryActivation` with a transfer entry, `navQueryActivation` (Profile build) with a `tick` entry, `pauseAfterWriterInput:true` with a `tick` entry, `pauseAfterWriterInput:true` with no transfer entry, non-Boolean `pauseAfterWriterInput`, non-integer `tick` | `ok:false` and unchanged `status` pending counts. `pauseAfterWriterInput:false` with a status-only batch and no recording returns `ok:true` and echoes `false` |
  | 6 | Replay: the rewritten `replay.md` `pauseAfterWriterInput` recording scenario, including scenario D's blaster probe, then the replay determinism check | automatic pause as the recipe states; the D blaster found by its `query_collection` row; CRC match and an `End replay ..., looping` marker |
  | 7 | Profile build: `navQueryActivation` arm with a `SpawnPlayer` batch | succeeds, with the `navQueryActivation` result object |
  | 8 | The old names `spawn_players`, `inject_status_changes`, `replay_transfer_fixture`, and `inject_outward_transfer` | `unknown command` (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.cpp:48`) |
  | 9 | On the finished tree, record the generated header's `Get-FileHash`, run `pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1`, and hash again | exit 0 and an identical hash; the file has no BOM, only LF line endings, and one trailing newline |
  | 10 | `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline 63669fbb61d2f1dbc0b16ae616c7ea661eb57a87 -IncludeUntracked` (the switch is needed only while the new files are untracked) | the `agent-field-names` row is `triggered:true` and `pass`; `validate-skill` and `markdown-links` rows still present |
  | 11 | Stale and malformed input, by code reading per the Change Workflow rule on unreachable paths (`.agents/references/change-workflow.md:122`): no temporary edit of `StatusChange.h` and no scratch copy, because the script has no input-path parameter | a data member added to any variant alternative adds one specialization line to the in-memory output, so `-Check` finds differing bytes and exits 1, and the row is `fail`; a missing variant declaration or a variant alternative with no struct definition exits 2, and the row is `blocked`; a header deleted by a diff still triggers the row through the both-sides scan |
  | 12 | Exact status change, single ticks: at timescale 1/32, pause and read `status.tick` = S; send one `SpawnPlayer` entry on an active coord with `tick:S+3`; step one at a time, and after each step read `status` and `query_players` on that coord while paused. If one step skips S+2 or S+3, repeat with a fresh target | while paused at S+2: the minted `globalIds[0]` is absent and `pendingAgentStatusChangeCount` is 1; while paused at S+3: it is present and the count is 0 |
  | 13 | Exact transfer, single ticks: same method, one `TransferPlayer` entry into an active coord with `tick:S+3` | while paused at S+2: the minted global id is absent from that coord's `query_players` and `pendingTransferFixtureCount` is 1; while paused at S+3: present and 0 |
  | 14 | Exact status change in multi-tick updates: one client connected. Choose a coord C from `status.activeCoords` before recording starts, so a replay writer records C's input from the first recorded tick (`Replay.cpp:964-965`); send `replay_record {"start":true}` and poll `recording:true`. Step `timescale {"faster":true}` until newly appended `ServerUpdate FullTicks: n (expected 1)` warnings with n ≥ 2 appear (`GameBase.cpp:342`). Confirm no row of C's `query_players` has a uuid in 900000000..900000015, read `status.tick` = S, then send one batch on C holding, for each i in 0..15, an `UpdatePlayer` probe with `tick:S+40+i` and `iPlayerUuid` 900000000+i, and a state-changing `SpawnPlayer` with `tick:S+40+i`. When `status.tick` > S+56, stop recording, poll `recording:false`, then run the replay determinism check | During recording: each probe's server `ProcessUpdateStatusChanges::kUpdatePlayer Uuid: 900000000+i NOT FOUND` line (`Players.cpp:336-339`) carries tick S+40+i, and C's `query_players` lists all 16 minted `globalIds`. At least one target lies on the second or later tick of a multi-tick update: a `FullTicks: n` warning stamped U with U+2 ≤ S+40+i ≤ U+n. If none does, repeat with a fresh S. No new `LogDifferences CRC Client` or `CONFIRMED DESYNC` line, so each spawn reached the client in its own tick's publication (`ServerBroadcaster.cpp:175-181`). During playback: each probe's NOT FOUND line appears again carrying tick S+40+i, proving replay captured each entry in its target tick's input; replay CRC match and an `End replay ..., looping` marker |
  | 15 | Past and current tick: on a paused server read `status.tick` = S, then send a `SpawnPlayer` entry with `tick:S`, then one with `tick:S-1`, then one with `tick:S+1` | the first two return `ok:false` with unchanged pending counts; the third returns `ok:true`, `deferred:true`, and `pendingAgentStatusChangeCount` +1; after unpausing, `query_players` lists its global id |
  | 16 | Clearing: send a `SpawnPlayer` entry and a `TransferPlayer` entry, both with `tick:status.tick+100000`, and confirm both pending counts rose, then `reset`. Then start recording, poll `recording:true`, send the same pair again, stop recording, and poll `recording:false`. Last, send `reset` again | after the first reset, both `status` pending counts are 0. After the recording stop, `pendingTransferFixtureCount` is 0 and `pendingAgentStatusChangeCount` still includes the `SpawnPlayer` entry. After the final reset, both pending counts are 0, so no far-future entry stays queued |
  | 17 | Held entries and release-time liveness, by code reading per `.agents/references/change-workflow.md:122` (a coordinate going inactive, or a destination losing liveness, between command and target is not reliably reachable from the harness) | `DrainPendingAgentStatusChanges` releases `iTick <= miTickCounter` per coordinate that passes the per-coordinate holds, so an entry whose coordinate was held at its target applies on the first later tick that can take it. `DrainReplayTransferFixtures` releases `iTick <= miTickCounter` and drops a released non-player entry whose destination fails `IsDestinationLive`, so `ApplyPreparedTransfers` never sees it (`ServerTransferManager.cpp:137-141`), and calls `PrepareReplayTransfers` for no coordinate whose released set is empty. With no queued entry, `PrepareTickStatusChanges` leaves the first tick's input and snapshot equal to the baseline's and copies nothing on later ticks (`### Per-tick release`) |

  Static checks also apply: Debug and Profile server builds compile (which also proves every listed member has a generated name and every listed member type has a parse); `git grep` finds no retired name outside `Documents/Investigations/MimallocPeakCommittedExceedsArenaReserve.md:13`; `git grep -e BuildElevationGrid -e MakeFrameElevationSampler -- Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp` and `git grep "terrain-clear Blaster position"` both find no match, proving the deleted search (`ServerSimulationFixtures.cpp:345-387`) is gone; and `git grep DrainPendingAgentStatusChanges` finds one call, inside `ServerBroadcaster::PrepareTickStatusChanges`.
- Roles (Tier 3):
  - `researcher` runs `/plan-alternatives`.
  - `reviewer` runs `/plan-audit` and `/plan-simplicity-review`; `/external-grill-plan` follows `/plan-audit`.
  - `implementer` runs `/implement-plan` in four slices:
    - the generator, the generated header from running it, the runner row, and the VS2026 `AGENTS.md` line;
    - the per-tick release (`ServerBroadcaster`, `ServerSession::PrepareTick`) and its two `AGENTS.md` lines;
    - the C++ command and the scheduled queues;
    - the harness and `Documents/` docs.

    Then `/update-vcxproj` for the new header, and `/update-affected-code`. The command slice starts after the generator slice has produced the header.
  - `mechanic` runs `/code-style-review`.
  - `builder` runs `/compile` (server Debug and Profile).
  - `reviewer` runs `/repo-code-review`, `/comment-review`, `/coherence-review` (docs and the two PowerShell scripts), and `/adversarial-review`.
  - `/update-claude-docs` and `/progressive-disclosure-review` run next.
  - The landing gate is `/finalize-changes`, with user confirmation.

## Classification evidence

Tier 3 (`.agents/references/risk-tiers.md:8-9`): a determinism-surface change and a change spanning independently owned subsystems.

- Determinism and tick-input assembly: the agent status-change drain and the broadcast snapshot move from the once-per-update `ServerBroadcaster::BuildFrameInputs` (`Engine/Source/Network/Server/ServerBroadcaster.cpp:65-80`, reached through `Engine/Source/GameBase.cpp:352`, `:959`) to every tick's `ServerSession::PrepareTick` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:43-66`, called at `GameBase.cpp:368`). That changes which tick an injected input enters — what the deterministic dispatch computes for a given schedule — and the per-tick contents of the inputs replay capture records (`Engine/Source/File/Replay.cpp:964-965`) and the publication clients check their CRC against (`ServerBroadcaster.cpp:173`, `:175-181`). `### Per-tick release` shows the bytes are unchanged when nothing is scheduled.
- Cross-subsystem: engine Network Server owns the broadcast snapshot and the agent drain (`Engine/Source/Network/Server/AGENTS.md:30`, `:35`); game Network Server owns per-tick preparation (`Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md:28-29`); the game Agent owns the fixture queues (`Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md:11`); and the static-check runner is shared tooling (`.agents/scripts/Invoke-StaticChecks.ps1:1-7`).
- Not touched: the wire and protocol (no `StatusChangeType` or payload change, `StatusChange.h:10-22`, `NetworkSerialization.cpp:12-185`); serialization and data layout (`StatusChange.h:55-153`, `FrameInput.cpp:40-80`; the target tick lives only in the in-memory fixture queues, which are never saved); save and replay compatibility (no version bump; replay capture stays keyed by destination, `ServerSimulationFixtures.cpp:956-959`); threading (all drains stay on the main thread, `GameBase.cpp:368`, `:516`; the `mCpuTimerMutex` transaction is reused, `ServerSimulationFixtures.cpp:641`); the trust boundary (the agent channel is an exempt developer tool, `Engine/Source/Agent/AGENTS.md:18`; the new checks sit inside that one unit); and build/bootstrap coordination (the runner is a per-session reporter that takes no lock and writes nothing, `Invoke-StaticChecks.ps1:2-7`; the vcxproj edit is one `ClInclude`, which has no build effect, `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:36`; the generator is never run by the build).

## Unresolved decisions

None. The agent-made choices are stated above as the author's recommendations with their rationale:

- the `PrepareTick` hook rather than a new call in `GameBase.cpp`;
- the new `PrepareTickStatusChanges` method holding the moved drain and snapshot;
- deleting the status drain's two whole-map holds;
- the `ScheduledStatusChange` element type;
- the omitted-`tick` target of `status.tick + 1`;
- accepting any `tick` above `status.tick` with no upper bound;
- applying a held entry on the first later tick that can take it;
- the release-time liveness drop for transfers;
- refusing `tick` with `navQueryActivation` or `pauseAfterWriterInput:true`;
- the `kfPlayerMaximumSpeed` velocity bound;
- counts that include scheduled entries;
- reuse of the existing clears;
- the generator details.
