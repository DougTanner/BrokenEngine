# Generic agent injection of simulation payloads into server frames

Open question: whether the agent harness should get one generic way to hand a
simulation payload (a `StatusChange` or a transfer) to a server frame, instead
of one hand-built fixture command per test need. This document records why the
question came up, inventories the four injection fixtures that exist, describes
the substrate they all share, compares three candidate shapes on the same
criteria, states a recommendation, and names the decisions a Plan needs.
Nothing here is implemented.

`AgentHarnessFineGrainedControl.md` already covers the neighbouring question of
scenario control — placing an enemy, forcing a shot, scheduling inputs by tick,
scenario fixtures in C++ — and is referenced here rather than repeated. Its
line references into `ServerSimulationFixtures.cpp` predate
`inject_outward_transfer` and have shifted; the ranges below are current.

## The problem

Each runtime acceptance criterion that needed a specific frame state has
produced its own server command with its own payload builder, its own
preconditions and refusals, and its own review round:

1. `spawn_players` — N default-placed players in one cell.
2. `inject_status_changes` — a batch of `SpawnPlayer`, `DestroyPlayer`,
   `UpdatePlayer`, or `UpdateFleet` entries, plus an optional profiler arm.
3. `replay_transfer_fixture` — one transfer arrival of any of the four types,
   only while recording, at a fixed reference position.
4. `inject_outward_transfer` — one player arrival placed so that it coasts
   out of the cell 16 ticks later.

All four end the same way: a `game::StatusChange` pushed into one of two
file-static queues that the engine drains on the next advancing update
(`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:24-31`).
The differences are entirely in the JSON schema, the payload defaults, the
geometry computed on the caller's behalf, and which refusals each command
applies. The fourth command was the trigger for asking whether the next need
should be a fifth.

## What exists today

| Command | Payload built | Queue and drain | Refusals at the command | Preconditions it computes for the caller | Build gate |
|---|---|---|---|---|---|
| `spawn_players` (`:677-721`) | `kSpawnPlayer` with `SpawnPlayerData` at the struct's default offset `[45,-12]` (`Frame/StatusChange.h:61-62`), fresh global id per entry (`:712`) | `QueueAgentStatusChange` (`:886-891`) → `DrainPendingAgentStatusChanges` | replay playback (`:679-682`); clients waiting for spawn (`:683-686`); `coord` not active (`:687-691`); `count` outside `[0,256]` (`:692-706`) | nothing beyond the id mint | none |
| `inject_status_changes` (`:514-675`) | one of four `StatusChange` kinds per entry through `BuildInjectedChange` (`:432-505`); `SpawnPlayer` mints the id at command time (`:452`), `UpdatePlayer` fixes the weapon countdown at one tick-rate (`:492`), `navigationDelay` is clamped like the wire (`:420-428`) | same queue and drain | exact top-level keys (`:520-526`); `changes` array (`:527-534`); nested `navQueryActivation` shape (`:536-564`); replay playback (`:568-571`); clients waiting for spawn (`:572-575`); profiler-arm state gates (`:576-610`); per-entry active `coord` (`:434-438`) and type whitelist (`:450-503`); `pos` shape (`:456-478`) | `pos` is validated for shape only; an out-of-cell spawn is refused where it is consumed (`Frame/Collections/Players/Players.cpp:295-300`) and the response still says `ok` (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:48`) | none for injection; `navQueryActivation` needs `kbProfiling` (`:578-581`) |
| `replay_transfer_fixture` (`:247-408`) | one `kTransfer*` with a full `TransferData` of fixture defaults: destination-cell centre, unit velocity along +x, health and shield 1, blaster type index, missile turn rate 2, fresh global id for `player` only (`:380-392`); the blaster branch searches a 20x20 grid for a terrain-clear point and materializes the destination's elevation grid to do it (`:336-378`) | `QueueReplayTransferFixture` (`:893-916`) → `DrainReplayTransferFixtures` | `kbDebugInput` (`:249-252`); replay playback (`:255-258`); recording active or a paused pending start (`:259-264`); `type` (`:265-272, 287-308`); `pauseAfterWriterInput` shape and double-arm (`:273-285`); `source`/`destination` distinct and Chebyshev-adjacent (`:310-315`); `source` active with both frames ready (`:316-332`); non-player destination live, inside the queue call (`:908-911`) | arrival position, terrain clearance for blasters, every `TransferData` default | `kbDebugInput` |
| `inject_outward_transfer` (`:726-825`) | one `kTransferPlayer` at the midpoint of the requested edge, inset by `(16 - 0.5)` tick-steps of `kfPlayerMaxSpeed`, velocity outward at `kfPlayerMaxSpeed`, health and shield 1, `fleetWantedCoord` = the seeded cell, fresh global id (`:788-815`) | same queue and drain as above (`:817-820`) | `kbDebugInput` (`:728-731`); replay playback (`:734-737`); clients waiting for spawn (`:738-741`); exactly two keys (`:742-749`); `delta` two integers in `[-1,1]`, not both zero (`:752-786`) | edge geometry, coast margin, velocity, armor above zero so the arrival is not flagged exploding (`:809-811`; `Frame/Collections/Players/PlayersCombat.cpp:281-286`) | `kbDebugInput` |

Unqualified `:line` ranges in this document are into
`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp`;
`Frame/...` paths are under `Projects/BrokenEngineSandbox/Source/`; a bare
`commands-server.md` is
`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`.

Two things the table makes visible:

- The refusal set is not uniform. `replay_transfer_fixture` lacks the
  clients-waiting-for-spawn refusal the other three carry, and its drain has no
  deferral either; both are already filed as
  `Documents/Plans/Game/AgentTransferFixtureSpawnWindow.md` and are not
  re-decided here. The exact-parameter-count idiom
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:11-33`)
  is applied by `inject_outward_transfer` and `inject_status_changes` but not
  by `spawn_players` or `replay_transfer_fixture`.
- Every command mints identity server-side. No caller supplies a global id, an
  alignment, or a client GUID; the fixture always takes
  `gpGame->GenerateGlobalId()` and `gpGame->PlayerAlignment()` (`:385, 390,
  452, 712, 802, 808`). Any generic shape has to keep those fields out of the
  caller's hands.

## The shared substrate

Both queues live in `ServerSimulationFixtureState`, bound to one live
`ServerSession` and cleared when the session changes (`:24-40`). Neither is
part of any `Frame`, so neither is CRC-checked, saved, or replayed as state; a
queued payload only becomes deterministic state once its drain moves it into
the same container real inputs use.

**Status-change queue.** `DrainPendingAgentStatusChanges` (`:918-965`) runs
from `ServerBroadcaster::BuildFrameInputs`
(`Engine/Source/Network/Server/ServerBroadcaster.cpp:82-90`), which
`PrepareActiveSet` calls after `ComputeActiveSet`
(`Engine/Source/GameBase.cpp:940-956`) at `Engine/Source/GameBase.cpp:347`.
That is after the agent command drain at `GameBase.cpp:309` and the boundary
poll at `GameBase.cpp:321`, so a command accepted in this update is drained into this update's
tick input. The drain holds the whole map when the update will not tick
(`:924-927`), during replay playback (`:928-931`), or while a client waits for
spawn (`:932-935`), and holds a single coordinate when it is inactive or has no
committed current frame (`:940-956`). What it consumes is appended to
`mFrameInputs[coord].statusChanges` and stable-sorted by type (`:957-962`),
which is the container the broadcaster snapshots for clients
(`ServerBroadcaster.cpp:92-99`) and the per-cell tick reads
(`GameBase.cpp:473-479`). `FinalizeFrameTick` clears it after the tick
(`GameBase.cpp:523-526`), so an entry is consumed exactly once.

**Transfer queue.** `DrainReplayTransferFixtures` (`:967-989`) runs inside
`ServerTransferManager::HarvestTransfers`
(`Engine/Source/Network/Server/ServerTransferManager.cpp:300-326`): after
`CollectTransfers` gathers the organic requests
(`ServerTransferManager.cpp:309`), before the deterministic sort (`:312`), the
replay capture (`:317-323`), and the apply (`:325`, all in
`ServerTransferManager.cpp`). `HarvestTransfers` is called from `FinalizeFrameTick` only on a
non-replay tick (`GameBase.cpp:510-513`), so a queued transfer waits for the
next finalized tick — paused means deferred — and there is no other deferral.
The drain creates the destination frame if it is missing (`:976-983`), which is
what lets a player fixture seed a cell that nothing else keeps alive:
`AddGameRequiredCoords` keeps any cell holding a player active
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:270-282`)
once `ComputeActiveSet` runs on the next update
(`Engine/Source/Network/Server/ServerSessionRuntime.cpp:312-327`), while
`SyncActiveFrames` deletes any frame outside that set
(`ServerSessionRuntime.cpp:287-310`). The queue
call itself refuses a non-transfer type, a wrong variant, or a non-player
destination that is not live (`:893-916`).

**What the drains guarantee, and what they do not.**

- Consumed once, on the same path as a real input or a real transfer, so the
  broadcast, the frame CRC, and the replay stream see the injected payload the
  same way they see an organic one. For transfers the destination CRC
  recompute is engine-owned and unconditional
  (`Engine/Source/Network/Server/AGENTS.md` `## Transfers and Publication`).
- Nothing about the payload's contents is checked at the drain. A status
  change is accepted as built; a transfer arrival is handed to `SpawnTransfer`
  (`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:13-103`), where a
  player arrival goes through `PlayersPostRender::Spawn` with an `ASSERT` that
  the position is inside the cell
  (`Frame/Collections/Players/Players.cpp:420-423`) and `ValidateVector` checks
  on position, direction, and velocity (`Players.cpp:428-430`). A transfer
  arrival
  outside the cell or with a bad W lane is therefore a server assert, not a
  refused command, whereas an out-of-cell `SpawnPlayer` is silently skipped
  (`Players.cpp:295-300`). A player arrival with zero health is flagged
  exploding on its first combat pass (`PlayersCombat.cpp:281-286`); every
  player arrival takes the one-second transfer lock, and player and spaceship
  arrivals take the arrival grace (`SpawnTransfer.cpp:89-90`, `:25`;
  `Frame/Collections/AGENTS.md` `## Game-Specific Rules`).
- The two drains differ in timing and deferral: status changes enter before
  the tick and are held while a client waits for spawn; transfers enter after
  the tick and are held only by pause. A generic injector inherits both
  behaviours as they are.

**Serialization shape.** The brief's premise that a JSON codec could be
derived from the serialization field list does not hold as stated.
`TransferData::SharedMembers` (`Frame/StatusChange.h:92-109`) is a nameless
`std::tie` used only for equality (`StatusChange.h:111-114`); the replay stream writes each
payload as raw trivially-copyable bytes
(`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp:20-55`;
`Common/Serialization.h:60-68`); and the wire codec is four hand-written
per-type subsets of `TransferData` with a matching per-type size table
(`Engine/Source/Network/NetworkSerialization.cpp:11-122, 129-153`). There is no
existing name-to-field mapping to reuse. A JSON codec would be a new name table
over the five payload structs (`Frame/StatusChange.h:54-164`), and the
per-type wire subsets are the natural statement of which `TransferData` fields
each transfer kind actually carries.

## Constraints every option must respect

- Determinism and CRC, replay format, the append-only type enum, and the
  hostile-parameter trust boundary — as stated in
  `AgentHarnessFineGrainedControl.md` `## Constraints every option must
  respect`. None of the shapes below adds a `StatusChangeType` or changes a
  payload byte, so no `FrameInput::kiVersion` or `kuiProtocolVersion` bump is
  in play; they change only how a payload is built at the command boundary.
- Every handler validates and throws
  (`Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts`).
  Widening what a caller can put in a payload widens what must be validated,
  because the drains and consumers do not.
- Server-side identity: global ids come from `gpGame->GenerateGlobalId()`,
  alignment from the session, and the client GUID stays empty
  (`commands-server.md:40, 50`). A payload field the caller could set to a
  colliding global id or a foreign alignment is an ownership exposure.
- `kbDebugInput`: the transfer queue is compiled out without it (`:895-898`)
  and both transfer fixtures refuse without it; `spawn_players` and
  `inject_status_changes` have no build gate today.

## Options

Each option is described by mechanism, what it adds and removes, and the
criteria from `/plan-alternatives` `### Comparison` that apply here: objective
fully met, net new code, invariant surfaces touched, reliance on existing
guarantees, and self-serving machinery.

### Option A — One generic `inject_payload` with a per-kind precondition table

- Mechanism: one server command, `inject_payload {"coord":[x,y],
  "kind":"SpawnPlayer"|...|"TransferMissile","fields":{...}}`, with an optional
  `entries` array form matching `inject_status_changes`. A name table maps
  JSON keys to the fields of each payload struct; `kind` selects the variant
  through `DefaultDataForType` (`Frame/StatusChange.h:174-184`) and the queue
  through `IsTransferType` (`StatusChange.h:29-32`). A small per-kind table
  states the
  server-owned fields the caller may not set (global id, alignment, client
  GUID; `uiPendingWeaponModeTicks` for `UpdatePlayer` if the current one-tick-
  rate behaviour is kept), the fields the kind's wire subset carries (so a
  missile `fields` object naming `fShield` is rejected), and the refusals:
  replay playback, clients waiting for spawn, active `coord` for status-change
  kinds, live destination for non-player transfers, in-cell finite position
  for every transfer (because the consumer asserts rather than refuses).
  Defaults are the struct defaults; an omitted transfer position is the cell
  centre as `replay_transfer_fixture` uses today.
- What it adds and removes: adds the name table, the kind table, and one
  handler; removes the four commands' payload builders and, if the specific
  commands are retired, their schemas and rows in `commands-server.md:40,
  48-50`. The blaster terrain search (`:336-378`) and the edge geometry
  (`:788-800`) leave C++ and become harness-script arithmetic against values
  the harness would have to obtain (see `## What a generic injector does not
  solve`).
- Objective fully met: yes — a new test need is a new `fields` object, not a
  new command.
- Net new code: the name table over five structs (about thirty names for
  `TransferData` alone) plus the kind table, against roughly four hundred
  lines of hand-built parsing and geometry removed.
- Invariant surfaces: no serialized format. The trust boundary widens from
  four fixed schemas to every payload field, so the command is
  `kbDebugInput`-only in full; that takes `SpawnPlayer` and `UpdatePlayer`
  injection away from a non-debug server unless the status-change kinds are
  left ungated, which is decision 4 below. The `pauseAfterWriterInput` arm
  (`:398-401`) and the `navQueryActivation` arm (`:624-664`) are side effects
  with their own state gates; they either become optional keys on the generic
  command or stay as the two commands that own them.
- Existing guarantees: relies on the drains, the consumer's out-of-cell spawn
  skip, and the transfer path's CRC recompute; must add the in-cell transfer
  check because nothing downstream refuses.
- Self-serving machinery: the name table exists only for the harness. It is
  the one piece of new machinery and it has to be kept in step with the
  structs by hand, since nothing enumerates their names.

### Option B — Generic codec plus the specific commands kept as sugar

- Mechanism: Option A's codec and kind table, with `spawn_players`,
  `replay_transfer_fixture`, and `inject_outward_transfer` kept as thin
  wrappers that fill defaults and geometry, then call the same builder and the
  same queue. `inject_status_changes` becomes the generic command's
  status-change form or stays as its own name.
- What it adds and removes: adds everything Option A adds; removes only the
  duplicated parsing inside the wrappers. The geometry and terrain search
  stay in C++ where they are reviewed and compiled.
- Objective fully met: yes for new needs; the existing needs keep their
  one-call commands.
- Net new code: Option A's plus the wrappers — the largest of the three.
- Invariant surfaces: same as Option A, plus two schemas for the same payload,
  so a precondition fixed in one place must be re-checked in the other. The
  `AgentTransferFixtureSpawnWindow` class of drift — one command missing a
  refusal its sibling has — is the current cost of exactly this shape.
- Existing guarantees: same as Option A.
- Self-serving machinery: the name table, plus wrappers whose only purpose is
  to spare the harness from arithmetic.

### Option C — Status quo with a shared helper library

- Mechanism: no new command. Extract what the four commands duplicate: the
  three-refusal prologue (build gate, replay playback, clients waiting for
  spawn), the exact-key check from `ServerFaultFixtures.cpp:11-33`, the
  `TransferData` default block, and a "position inside this cell and finite"
  check for transfers. Each future need is still a new command, but it is
  built from those helpers.
- What it adds and removes: adds the helpers; removes the duplicated blocks
  from the four commands. Nothing leaves C++.
- Objective fully met: no — the fifth need is still a fifth command and a
  fifth review round; the cost per command drops, the count does not.
- Net new code: smallest.
- Invariant surfaces: none new; each command keeps its narrow schema, so the
  hostile surface stays fixed and each command can stay on its current build
  gate.
- Existing guarantees: unchanged reliance.
- Self-serving machinery: none.

## Comparison

| | A generic command | B codec plus sugar | C helpers only |
|---|---|---|---|
| Objective fully met | yes | yes | no |
| Net new code | name table and kind table; four builders removed | name table, kind table, and wrappers | helpers only |
| Serialized formats or versions touched | none | none | none |
| Hostile-parameter surface | every payload field, `kbDebugInput`-only | same, plus the narrow schemas | four fixed schemas |
| Geometry and terrain search | harness scripts | C++ wrappers | C++ commands |
| Preconditions live in | one kind table | kind table and each wrapper | each command |
| Places a refusal can drift | one | two per need | one per need |
| Self-serving machinery | name table | name table and wrappers | none |
| Fifth need costs | a `fields` object | a `fields` object, or a wrapper | a command and a review |

## Recommendation

Take Option A. The objective is that the next test need should not be a new
server command, and only A delivers that without keeping a second surface
alive. Its real cost is not the codec, which is a name table over five structs
that already exist, but two consequences that the decision below has to accept
explicitly: the geometry a fixture computes today moves to the harness, and
the whole command becomes `kbDebugInput`-only because arbitrary payload fields
cannot be offered to a non-debug server.

Do not take B: it reproduces the drift the current shape already suffers from
(`AgentTransferFixtureSpawnWindow.md`), and a wrapper that exists to spare the
harness one subtraction is a command by another name. Take C only if the user
decides the hostile-surface widening or the loss of in-C++ geometry is
unacceptable; it is cheaper than A but leaves the problem statement true.

Land `Documents/Plans/Game/AgentTransferFixtureSpawnWindow.md` and
`Documents/Plans/Game/AgentTransferQueueStatusCounts.md` independently. The
first fixes the transfer drain's deferral and the fixture's refusal, which
Option A inherits unchanged; the second gives any injector the queue-depth
read-back its acceptance rows will need. Neither depends on this decision.

## What a generic injector does not solve

- Geometry helpers leave C++. The edge placement in `inject_outward_transfer`
  needs the cell half-extent from `engine::LocalFrameArea()`, `kfPlayerMaxSpeed`
  (`Frame/Collections/Players/Players.h:32`), and `engine::kfDeltaTime`; the
  blaster placement needs the destination's elevation grid. None of these is
  visible to the harness today. Under Option A the harness either receives
  them through a query (a `status` or `query_frame` field for the cell extent
  and tick step; a terrain probe for blasters) or the harness author hard-codes
  constants that silently go stale. The blaster case is the harder one: a
  terrain-clear search is not arithmetic a script should do.
- Timing-sensitive preconditions remain. The transfer drain runs after the
  tick and the status-change drain before it; a clients-waiting-for-spawn
  window can open between the command and its drain
  (`AgentTransferFixtureSpawnWindow.md` `## Context`); a `pauseAfterWriterInput`
  arm depends on recording state (`:259-264, 282-285`); an arrival still
  takes a one-second lock and grace. A generic command changes none of this,
  and its documentation row has to say so for every kind.
- The hostile-parameter surface widens and must stay `kbDebugInput`-only. A
  caller-chosen `TransferData` can name a non-finite vector, a W lane of the
  wrong value, an out-of-cell position, a zero armor, a colliding global id,
  a foreign alignment, or an arbitrary `uiPlayerFlags` word. The first three
  are asserts in the consumer, the next two are silent misbehaviour, and the
  last two are ownership exposures; the kind table must refuse or overwrite
  each before queueing, because nothing downstream will.
- Schema maintenance. Because no field-name list exists, the name table is
  hand-kept; adding a member to a payload struct without adding its name is a
  silent gap the harness discovers only when it needs the field. The
  per-type wire subsets (`NetworkSerialization.cpp:129-153`) are the closest
  existing statement of which fields each transfer kind carries and are the
  natural cross-check.

## Decisions a Plan needs

1. Where preconditions live — this is the deciding question. Three
   candidates: the caller (the harness script is responsible for an in-cell
   position and the command trusts it, which the consumer assert makes
   unacceptable for transfers); engine asserts (accept a server crash as the
   refusal, which contradicts the hostile-input contract); or a validation
   table in the command, one row per kind (the shape Option A assumes). A
   Plan cannot be written until this is fixed, because it determines what the
   kind table contains.
2. Whether the four specific commands are retired, and in what order. Retiring
   `spawn_players` and `inject_status_changes` changes documented harness
   contracts (`commands-server.md:48-49, 52`) and every scenario that uses
   them; retiring `replay_transfer_fixture` has to carry its
   `pauseAfterWriterInput` arm somewhere; retiring `inject_outward_transfer`
   needs the geometry inputs in item 5 first.
3. The server-owned field list per kind: at least the global id, the
   alignment, and the client GUID; whether `uiPendingWeaponModeTicks` and
   `uiPendingFleetWantedCoordTicks` keep their fixture values (`:492, 498`) or
   become caller-settable; whether `fleetWantedCoord` defaults to `coord`.
4. Build gate: the generic command is `kbDebugInput`-only in full, or the
   status-change kinds stay available on a non-debug server as
   `inject_status_changes` and `spawn_players` are today. The transfer queue
   itself is compiled out without `kbDebugInput` (`:895-898`), so transfer
   kinds cannot be ungated.
5. How the harness obtains the geometry inputs: cell extent and tick step
   through `status` or `query_frame`, a terrain-clear probe for blaster
   placement, or none of these (accepting that only centre-placed payloads are
   expressible without hard-coded constants).
6. Whether the `pauseAfterWriterInput` and `navQueryActivation` arms become
   optional keys on the generic command or stay as separate commands that only
   arm and do not inject.
7. Response shape: the minted global ids and the `deferred` flag every current
   command returns (`:718-720, 822-823`), and whether a transfer kind reports
   that its destination was created.

The next step is the user fixing items 1 and 2. With those two answered the
remaining items are implementation choices, and the result is a
decision-complete Plan under `Documents/Plans/Game/` naming
`ServerSimulationFixtures.cpp`, `commands-server.md`, and the
`Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts` fixture
sentences as its critical files.
