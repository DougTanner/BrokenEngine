# Fine-grained harness control of simulation scenarios

Open question: how the agent harness can set up a specific simulation
situation on demand — the motivating case is "a player fires a missile at a
cell edge" — so that runtime acceptance criteria phrased that way become
drivable instead of `BLOCKED`. This document records the gap, what the code
offers today, four candidate mechanisms compared on the same criteria, a
recommendation, and the decisions a Plan needs before any of it is executable.
Nothing here is implemented.

## The gap

What the harness can do today, all on the server:

- Place players: `spawn_players` and `inject_status_changes` with
  `SpawnPlayer`, which accepts a `pos` offset in meters from the cell center
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:48-49`).
- Change a player's weapon mode and navigation delay through `UpdatePlayer`,
  and its fleet target through `UpdateFleet` (same lines). Every injected
  change is a `StatusChange` that the broadcaster drains into the per-cell
  tick input on the next advancing update, so it rides the same broadcast,
  CRC, and replay channel as a real spawn
  (`Engine/Source/Network/Server/ServerBroadcaster.cpp:82-99`;
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:808-855`).
- Pause, step time, save, load, reset, and record or play a replay
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:33-43`).
- Fabricate one cross-cell transfer of a player, spaceship, blaster, or
  missile into an adjacent cell — but only while recording or with a pending
  recording start, and only at a fixed reference position in the destination
  cell (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:255-264, 334`).
- Read counts and rows back with `query_frame`, `query_players`, and
  `query_collection` (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:44-46`).

What the missile path needs, from the current code:

- A player sets its own `kFireMissile` flag only when `AcquireTarget` finds a
  live, non-grace, visible enemy spaceship within `kfMissileTargetRange`, 160
  m (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:24-25, 101-158`).
- `SpawnMissiles` consumes that flag and additionally requires `kUseMissiles`
  and an expired cooldown (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:387-408`). `kUseMissiles` arrives
  through an `UpdatePlayer` status change and only takes effect after a
  one-second countdown (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:361-381, 790-804`;
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:487-492` sets the countdown to one tick-rate).
- Enemy spaceships appear only from the frame's group spawner, which picks a
  grid-scored anchor at least 120 m from every live player and prefers about
  150 m, once per half second per cell
  (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:15, 248-426, 447-452`).
  Nothing lets a command choose where an enemy appears, and nothing forces a
  shot.

So the only primitive genuinely missing for the motivating criterion is "put
an enemy spaceship at a chosen position in a chosen cell, outside recording".
Player placement, weapon mode, pause-for-exact-tick, and the read-back
queries already exist.

Evidence from this session (scratch artifacts under `Temp/`, not tracked):

- `Temp/missile-edge-attempt1.json`, selectors `samples` and `finalTotals`:
  eight AI players injected at cell edges, then 55 seconds of simulation at
  raised timescale produced enemy spaceships and blasters in the hundreds but
  zero missiles in any cell. Caveat for anyone re-citing this run: its
  `useMissiles` selector holds a failed `inject_status_changes` request (a
  JSON type error), so `kUseMissiles` may never have been set in that run.
  The range gate above is established from the code, not from that artifact
  alone; a clean rerun with an accepted `UpdatePlayer` batch is needed before
  the artifact is cited as the sole evidence.
- `Temp/missile-transfer-attempt2.json`, selectors `captureAtEvent`,
  `captureAfterUnpause`, `captureAfterPlayback`: a fixture missile transfer
  latched `missileCount` 1 at its event tick, an organic blaster transfer then
  replaced it, and playback latched the organic event. Cause and fix are
  already filed as
  `Documents/Plans/Engine/ReplayTransferCaptureFixtureEventPin.md`
  (`Engine/Source/Agent/Commands/ReplayFixtures.cpp:274-296` latches the
  last event unconditionally). That Plan stands on its own and is not
  re-decided here.

## Constraints every option must respect

- Determinism and CRC: PostRender state is CRC-checked per tick and every
  CRC-affecting decision must come from `Frame`, `FrameInput`, static data, or
  phase parameters (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`
  `## Invariants`). Mutating a frame from the command drain outside the tick
  input would broadcast nothing, so every connected client desyncs and every
  replay checksum breaks. Any new scenario control must therefore enter as a
  `StatusChange` in the tick input or as a transfer in the post-dispatch
  channel.
- Replay format: `FrameInput` is the versioned per-tick unit; a change to the
  `StatusChange` payload bytes a server writes bumps `FrameInput::kiVersion`,
  and a wire-visible change also bumps `engine::kuiProtocolVersion`
  (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants`; `Engine/Source/Network/AGENTS.md`
  `## Transport Contracts`). The type enum is append-only
  (`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:6-20`). Replay
  playback rejects a transfer inside a recorded input and a non-transfer
  inside a post-dispatch record (`Engine/Source/File/Replay.cpp:1072-1095`).
- Trust boundary: command parameters are hostile input and every handler
  validates and throws (`Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`
  `## Contracts`); replay files are a trust boundary at every read
  (`Engine/Source/File/DifferenceStream.h:294-300, 311-313, 437-452`).
- Transfers: destination must be Chebyshev-adjacent, a non-player destination
  must already be live, and the destination frame's shared CRC is recomputed
  after arrival (`Engine/Source/Network/Server/AGENTS.md`
  `## Transfers and Publication`). A transferred spaceship carries a fresh
  one-second arrival grace during which the player's acquire path refuses it
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`
  `## Game-Specific Rules`; `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:107-109`).
- Harness policy: a missing primitive returns the criterion `BLOCKED` and
  names the narrowest extension; the main agent decides whether the change
  includes it (`.agents/skills/agent-harness/SKILL.md:55`).

## Options

Each option is described by mechanism, critical files, what it adds,
invariant surfaces, and when it pays off.

### Option 1 — Narrow server commands

Two separate primitives are bundled under this heading because they differ
sharply in cost.

**1a. Force a named player's fire flag next tick.**

- Mechanism: a new field on `UpdatePlayerData` (or a new appended
  `StatusChangeType`) consumed where `kUpdatePlayer` is applied in the Players
  Update phase (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:361-381`), setting `kFireMissile` on the row.
  `AcquireTarget` only ever sets the flag, and `SpawnMissiles` clears it in the
  Spawn phase, so a flag set during Update survives to the shot.
- Critical files: `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:72-79` (payload), `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:11`
  (version), the game Network codec that writes `UpdatePlayerData` on the
  wire, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:361-381`, `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:487-492`,
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:48`.
- What it adds: a shot on demand. Without a target inside 160 m the missile
  leaves along the hull direction with no homing target
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:420-425`), so this proves "a missile spawned at the
  edge", not "fired at" something.
- Invariant surfaces: payload bytes change, so `FrameInput::kiVersion` and
  `kuiProtocolVersion` both bump and the client must read the new field;
  stale replays and saves are invalidated by design. Trust boundary is the
  ordinary parameter validation. `kUseMissiles` and the cooldown still gate
  the shot, so the harness still sends `UpdatePlayer` and waits one second.
- When it pays off: only if a criterion needs a targetless shot or a shot at
  an exact tick regardless of enemy position. Otherwise it is a speculative
  verb with two version bumps.

**1b. Place an enemy spaceship at a position.**

- Mechanism: the transfer path already materializes a spaceship at a given
  position in a destination cell with a full `TransferData` — that is exactly
  what `replay_transfer_fixture` builds and queues
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:379-395`), and the queue drains through the
  transfer manager into the ordinary harvest, sort, capture, and apply path
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:857-879`). The recording requirement lives
  only in that command's validation (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:259-264`),
  not in the mechanism; `QueueReplayTransferFixture` itself gates on
  `kbDebugInput` and a live destination (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:783-806`).
  The narrow change is a placement command (or a relaxed fixture) that takes
  a `pos` in the same meters-from-cell-center form `SpawnPlayer` uses and does
  not require recording. The blaster branch already shows the terrain-clear
  search a placed entity needs (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:335-377`).
- Critical files: `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:247-407, 783-806`;
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:40`; `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`
  `## Contracts` (fixture sentences).
- What it adds: the one missing primitive. With a player at an edge
  (`SpawnPlayer pos`), `UpdatePlayer useMissiles`, a one-second wait for the
  weapon countdown, and a spaceship placed within 160 m, the ordinary
  `AcquireTarget` fires without any forced flag, and `query_collection
  missiles` shows the row with a nonzero `registryTargetId`
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:46`).
- Invariant surfaces: none of the serialized formats change — the transfer
  rides the existing post-dispatch channel and existing `TransferData`. The
  entity gets arrival grace, so the harness waits one second after placement.
  The command stays server-only and `kbDebugInput`-gated like the fixture it
  reuses. Trust boundary: `pos` must be validated in-cell and finite, and the
  refuse-outside-the-cell rule applies at the consumer.
- When it pays off: immediately, for the motivating criterion and for any
  "enemy at a chosen spot" scenario, at the cost of one command and one
  documentation entry.

### Option 2 — General scripted-input command

- Mechanism: extend `inject_status_changes` into a script: each entry carries
  an `atTick`, the pending map in `ServerSimulationFixtureState`
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:24-31`) holds entries until that tick, and
  `DrainPendingAgentStatusChanges` releases them into the tick input the
  deterministic dispatch reads (`Engine/Source/GameBase.cpp:452-505` builds
  the per-cell `pFrameInput` reference; `Engine/Source/GameBase.cpp:507-527` clears inputs
  after the tick).
- Critical files: `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:24-31, 513-674, 808-855`;
  `Engine/Source/Network/Server/ServerBroadcaster.cpp:82-90`; `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:48, 51`.
- What it adds: exact-tick scheduling of many steps in one request. It does
  not add vocabulary: the sandbox has no human-steered player, `FrameInput`
  instances are never built from hardware (`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants`),
  and the only "player input" is the `StatusChange` set. Every new verb —
  fire, place enemy — still needs Option 1's changes.
- Invariant surfaces: scheduling alone touches no format. Deferred entries
  already survive pauses and apply on the next unpaused tick
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:51`), so "pause, inject, unpause" already gives
  exact-tick placement without a scheduler.
- When it pays off: only for long multi-step scripts where per-step round
  trips at high timescale would miss their tick. For a handful of steps the
  pause-and-inject pattern is equivalent and needs no code.

### Option 3 — Agent-constructed replay

The proposal: a skill assembles a replay offline (initial state plus per-tick
inputs and status changes), the harness plays it back, and the main session
inspects logs afterwards.

**What a replay contains.** A recording is a generation of files under the
server AppData root (`Engine/Source/File/AGENTS.md` `## Replay Streams`):

- `F7.replay.manifest`, v4: version, initial tick, activation records, a
  full-frames flag, an inventory of every artifact with byte count and
  SHA-256, and a generation digest over the payload
  (`Engine/Source/File/Replay.cpp:19, 92-100, 414-570`; the exact byte layout and a repair
  procedure are in `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:80-84`).
- `F7.replay.grid`: a whole-grid save written at recording start
  (`Engine/Source/File/Replay.cpp:786-787`), versioned by `Frame::kiVersion` and adopted only
  after the whole stream validates (`Engine/Source/File/AGENTS.md` `## Grid Saves`).
- `F7.replay.meta`: game metadata, written after the writer files and read
  into a staged value the engine applies only after the generation validates
  (`Projects/BrokenEngineSandbox/Source/Save/AGENTS.md` `## Replay Contract`).
- Per cell and activation, `F7.replay.<coordKey>.<activationTick>`: version
  headers for `Frame` and `FrameInput`, the complete serialized saved start
  frame, the initial input, the difference count, the complete serialized
  saved end frame, and the post-dispatch (transfer) records; a `.frames`
  sibling holding `(tick, FrameInput)` only on ticks whose input differs from
  the previous one; a `.checksums` sibling holding one frame CRC per tick of
  the span; and on Debug a `.fullframes` sibling
  (`Engine/Source/File/DifferenceStream.h:24-52, 54-74, 78-81, 83-150`).

**How inputs are represented.** `FrameInput` is a vector of `StatusChange`
serialized as a count followed by a type byte and its payload
(`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:9-19`; `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp:20-55`). Ordinary inputs (spawn,
destroy, update player, update fleet) live in `.frames`; transfers live only
in the post-dispatch records (`Engine/Source/File/Replay.cpp:1072-1095`). There is no per-tick
player control record beyond those types — the same vocabulary limit as
Options 1 and 2, so an authored replay still cannot say "fire" unless a new
`StatusChange` verb exists.

**Can an agent author one without running the game?** Not from scratch:

- The header requires complete serialized saved start and saved end frames
  (SOA collections, RNG state, timers). Only the simulation produces those,
  and the reader validates the end frame's tick against the start
  (`Engine/Source/File/DifferenceStream.h:282-300`).
- The checksum file must hold exactly one CRC per tick of the span or the
  reader refuses to load (`Engine/Source/File/DifferenceStream.h:437-449`); the empty-checksum
  early return (`Engine/Source/File/DifferenceStream.h:528-531`) is unreachable through a valid
  load. Those CRCs are the result of resimulating the authored inputs, which
  again requires running the game.
- The manifest binds every artifact by SHA-256 and a generation digest
  (`Engine/Source/File/Replay.cpp:546-570`), so any authored byte needs the repair procedure in
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:80-84`.

What is achievable is record-then-splice: record a real baseline, edit the
`.frames` input stream, repair the inventory and digest, and play it.
`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:75` already performs a one-byte version of this by hand. The
consequences of splicing:

- Playback applies the spliced inputs through the ordinary path and
  broadcasts them (`Engine/Source/File/Replay.cpp:1063-1105`), so the scenario does run.
- Every tick after the splice resimulates to a state whose CRC no longer
  matches the recorded one. `ValidateChecksum` only logs the mismatch
  (`Engine/Source/File/DifferenceStream.h:560-570`), so playback continues, but each tick emits
  the `LogDifferences CRC Client` error that replay acceptance forbids
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:15`). The determinism check — the reason to use a replay as
  evidence — is forfeited unless a second recording pass regenerates the
  checksums, and recording cannot start while playing (`Engine/Source/File/Replay.cpp:782-784`
  clears readers), so that pass is a fresh live run, at which point the
  splice bought nothing over live injection.
- Injection is rejected during playback (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:567-570`),
  so the authored file is the whole scenario; nothing can be adjusted live.

**Scratch log level.** `kTemp` already exists as a log category, not a level:
`LogCategory::kTemp` (`Common/Log/LogTypes.h:18-32, 46`) with a compile floor
and runtime default of `kVerbose` in every build so it always emits
(`Common/Log/Log.cpp:22-33`; `Common/Log/AGENTS.md:7`), a `kTemp: ` line
prefix that makes it grep-able (`Common/Log/Log.h:156-162`), and a hub rule
reserving it for transient diagnostics (`Common/AGENTS.md:35`). Levels are
the fixed five in `Common/Log/LogTypes.h:6-14`. Nothing new is needed; the caveat is
that a `LOG(kTemp, ...)` is a tracked source edit compiled into the binary,
so a per-scenario line is a temporary code change the workflow has to land or
revert. A durable signal belongs on the owning category at `kDebug`, like the
existing muzzle-outside-cell line (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:440-444`), and the
queries already expose the missile row, which is cheaper than any log
(`.agents/skills/agent-harness/references/worker.md:548-553` prefers queries
over logs and pixels).

- Critical files if pursued: a new skill under `.agents/skills/`, a manifest
  repair script (the procedure in `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:80-84`), `Engine/Source/File/DifferenceStream.h`,
  `Engine/Source/File/Replay.cpp:392-750` (adoption), `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp`.
- What it adds: an offline, re-playable scenario file — but only as an edit
  of a recording the game already produced, and only over the existing
  `StatusChange` vocabulary.
- Invariant surfaces: no format change for pure splicing; the replay trust
  boundary and manifest integrity are exercised as intended; the determinism
  evidence is lost for the spliced span.
- When it pays off: for regression fixtures that must run with no live
  commands at all, after Option 1b exists to record the baseline. Not for
  driving an acceptance criterion in a session.

### Option 4 — Harness-side scripted scenario fixture

- Mechanism: a named scenario command in `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp` (for
  example `scenario_fixture {"name":"missile_at_cell_edge"}`) that sequences
  the existing primitives in C++: queue `SpawnPlayer` with an edge `pos`,
  queue `UpdatePlayer useMissiles`, queue a spaceship transfer at a position
  inside 160 m through the fixture queue, and optionally arm the same
  writer-input pause `replay_transfer_fixture` uses
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:397-400`; `Engine/Source/Agent/Commands/ReplayFixtures.cpp:240-267`)
  so the harness observes the exact tick.
- Critical files: `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:247-407, 724-774, 776-806`;
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`; `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`
  `## Contracts`.
- What it adds: same-tick ordering and one-call setup without exposing a
  general placement command. Each scenario is a reviewed C++ change and its
  semantics are invisible unless documented in `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`.
- Invariant surfaces: identical to Option 1b — no format change if the
  fixture only queues existing `StatusChange` and transfer types; the same
  `kbDebugInput` gate.
- When it pays off: when a criterion depends on ordering that separate
  commands cannot pin (two events in one tick, or an event on the exact tick
  recording starts), which is the case `replay_transfer_fixture` exists for.
  For the missile case the ordering is loose (one-second waits dominate), so
  a fixture would hard-code what three ordinary commands express.

## Comparison

| | 1a force fire | 1b place enemy | 2 scripted input | 3 authored replay | 4 scenario fixture |
|---|---|---|---|---|---|
| Solves the motivating criterion | shot only, no target | yes | no (needs 1b) | no (needs 1b and a recording) | yes, one scenario |
| New `StatusChange` or payload bytes | yes | no | no | no | no |
| `FrameInput::kiVersion` / `kuiProtocolVersion` bump | both | none | none | none | none |
| Replay format change | no | no | no | no (splice only) | no |
| Determinism evidence preserved | yes | yes | yes | no for the spliced span | yes |
| Client build change | yes | no | no | no | no |
| Reusable across scenarios | narrow | broad | broad scheduler, no verbs | fixture files only | one per scenario |
| Cost | two version bumps plus codec | one command plus docs | scheduler plus docs | skill plus repair tooling | C++ per scenario |

## Recommendation

Take Option 1b first, and only 1b:

- It is the single missing primitive. Player placement (`SpawnPlayer pos`),
  weapon mode (`UpdatePlayer useMissiles`), exact-tick placement
  (pause, inject, unpause), and read-back (`query_collection missiles` with
  `registryTargetId`) already exist, so once an enemy can be placed within
  160 m of an edge player the ordinary acquire path fires a targeted missile
  and the criterion reads "fired at", not merely "spawned".
- It changes no serialized format and no client code, because the transfer
  path already carries a complete spaceship and already recomputes the
  destination CRC.
- It reuses the fixture code that exists for the same job, with the
  recording gate and the fixed position being the only things in the way.

Defer 1a until a criterion demands a targetless or exact-tick shot; it costs
two version bumps for a verb the motivating criterion does not need. Skip
Option 2's scheduler: pause-and-inject already pins a tick, and the option adds
no vocabulary. Do not pursue Option 3 as authoring: the format needs complete
frames and per-tick CRCs only the simulation can produce, and a spliced
replay plays but forfeits the checksum evidence that makes a replay worth
using. The record-then-splice form is already documented for the one case
that needs it. Reserve Option 4 for same-tick ordering, where
`replay_transfer_fixture` is the precedent.

Land `Documents/Plans/Engine/ReplayTransferCaptureFixtureEventPin.md` independently; it fixes the
observation register, not the scenario control, and neither blocks the other.

## Decisions a Plan needs

1. Command shape: a new placement command, or `replay_transfer_fixture` with
   its recording gate removed and a `pos` parameter added, keeping
   `pauseAfterWriterInput` meaningful only while recording. Naming follows
   from this: the repository term for the mechanism is transfer, and the
   command should say what it does.
2. Position semantics: meters from the destination cell center, matching
   `SpawnPlayer pos` (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:48`), and the refusal rule for an
   out-of-cell, non-finite, or terrain-blocked position — reject at the
   command, or log and skip at the consumer as `SpawnPlayer` does. The
   blaster terrain search (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:335-377`) is the
   existing precedent for a placed entity.
3. Type coverage: spaceship only, or all four transfer types the fixture
   already builds. The `TransferData` defaults at
   `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:379-391` are fixture values (health 1,
   shield 1, unit velocity) and a Plan must state whether they are acceptable
   for a placed enemy.
4. Arrival grace: accept the one-second wait after placement, or exempt placed
   entities. Exemption touches the shared arrival-grace contract in
   `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md` and is out of proportion for a harness wait.
5. Whether a targetless forced shot (1a) is ever a criterion. If yes, decide
   the payload field versus a new appended type, and schedule the
   `FrameInput::kiVersion` and `kuiProtocolVersion` bumps.
6. The runtime evidence rule for "fired": polling `query_collection missiles`
   for a row with nonzero `registryTargetId` versus a durable `kDebug` line in
   `SpawnMissiles`; and an explicit statement that `kTemp` lines never land in
   tracked code for a scenario.
7. Build gate: keep the placement `kbDebugInput`-only like the fixture it
   reuses (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:785-788`), or make it always
   available like `inject_status_changes`.
8. Documentation owners: the new `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` entry, the fixture
   sentences in `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`
   `## Contracts`, and whether the harness `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` scenarios that say
   "do not add a fixture API" (`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:69`) need rewording.
9. A clean rerun of the edge scenario with an accepted `UpdatePlayer` batch,
   so the Plan's context cites an artifact that isolates the range gate.
