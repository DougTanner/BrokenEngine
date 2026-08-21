<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T18:49:28.719Z","dependsOn":[]} -->
# Harness unit placement and pusher zone-overflow observability

## Context

`Documents/Plans/Engine/PusherCellWideCoverage.md` converts the pusher
broadphase from a 400 m arena anchored on one entity into cell-wide coverage.
Two of its acceptance criteria cannot be observed with today's agent harness,
so that Plan depends on this one.

Missing capability 1 — place a unit at a chosen point in a cell. The pusher
Plan requires "a harness scenario places two players in one cell roughly 600 m
apart". Every player the harness can create spawns at exactly one point:
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:290-294`
computes the cell center from `rStaticData.vecArea` and spawns at
`center + (45, -12)` with `z = engine::gBaseHeight.Get()`, for every
`kSpawnPlayer` status change without exception. No harness command writes a
position: `spawn_players` takes `{coord,count,isFlagship}` with `count` capped
at 256 (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:964-994`),
`inject_status_changes`'s `SpawnPlayer` takes only `isFlagship` and
`fleetWantedCoord` (`AgentCommandsServer.cpp:777-785`), its `UpdatePlayer`
takes only `useMissiles` and `navigationDelay` (`AgentCommandsServer.cpp:791-796`),
and `query_players`/`query_frame`/`query_collection` are read-only. Two players
in one cell are therefore always spawned on top of each other, and nothing can
separate them by a chosen distance.

Missing capability 2 — see a pusher per-zone overflow. The pusher Plan keeps
the 512-entry per-zone cap and states that overflow "stays loud" and must be
absent in the acceptance runs. It is not loud today:
`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:85-89` drops the
513th registration into a zone with `DEBUG_BREAK(); continue;` and logs
nothing, and `DEBUG_BREAK()` (`Common/ErrorUtils.h:11`) expands to
`if constexpr (kbDebugBreak) { if (IsDebuggerPresent() == TRUE) { __debugbreak(); } }`,
so in every harness run — which launches the executables without a debugger —
the drop is completely silent. The pusher Plan's cell-wide zones are 18 m
instead of 8 m on a side, reaching that cap at roughly a fifth of the density,
which makes the missing signal materially more likely to matter.

This Plan adds exactly those two capabilities and nothing else.

## Design

### 1. `SetPlayerPosition` status change and its harness surface

Add one appended status-change type that moves an existing player to a chosen
world point, and expose it through the existing injection command rather than a
new top-level command, because `inject_status_changes` already owns coordinate
validation, the replay and spawn-wait rejections, the paused/`deferred`
response, and the queue path (`AgentCommandsServer.cpp:763-952`). A second
command would duplicate all of it.

1. `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`: append
   `kSetPlayerPosition` immediately before `kCount` (the enum's own comment
   requires append-only), add its `StatusChangeTypeName` case
   `"SetPlayerPosition"`, add

   ```cpp
   struct SetPlayerPositionData
   {
       int64_t iPlayerUuid = 0;
       float fX = 0.0f;
       float fY = 0.0f;
       bool operator==(const SetPlayerPositionData&) const = default;
   };
   ```

   as a new `StatusChangeData` variant arm, and return it from
   `DefaultDataForType`. Two floats, not an `XMVECTOR`: `z` is not a free
   parameter — every player is held at `engine::gBaseHeight.Get()` by
   `PlayersInterpolate::Update`
   (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:504`),
   so a supplied `z` would be overwritten on the next tick and would only
   mislead.
2. `Engine/Source/Network/NetworkSerialization.cpp`: add the type to
   `StatusChangeItemWireSize` (`kiI64 + 2 * kiF32` = 16 bytes, far below the
   existing `kiMaxStatusChangeBytesPerItem = 120` in
   `Engine/Source/Network/NetworkSerialization.h:18`, which therefore does not
   change) and to the `SerializeGroup` and `DeserializeStatusChangeBatch`
   switches, writing and reading `iPlayerUuid`, `fX`, `fY` in that order. The
   file's comment at `NetworkSerialization.cpp:144-148` names exactly this set
   of sites.
3. Version and protocol bumps, both required by
   `Projects/BrokenEngineSandbox/Source/Network/AGENTS.md:14-15`: bump
   `FrameInput::kiVersion` from 15 to 16
   (`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:11`) so older
   replays are rejected instead of misread, and bump
   `engine::kuiProtocolVersion` from 11 to 12
   (`Engine/Source/Network/NetworkProtocol.h:63`) so a mismatched build is
   rejected at Hello. Build-local replay `Write`/`Read`
   (`Frame/FrameInput.cpp:20-55`) needs no new code: it visits the variant, and
   the new arm is a plain aggregate of trivially serializable fields.
4. Consumption, in `PlayersPostRender::ProcessUpdateStatusChanges`
   (`Players.cpp:312-370`), as a third branch beside `kUpdateFleet` and
   `kUpdatePlayer`, so the write happens in the PostRender Update phase on
   client and server alike from the same `FrameInput`:

   - Look the player up in `rCurrentInterpolate.idToIndexMap` by
     `player_t {engine::uuid_t {rSet.iPlayerUuid}}`, exactly as the
     `kUpdatePlayer` branch does; a miss logs the same style of `kNetwork`
     `kWarning` line and is otherwise ignored.
   - Build `XMVECTOR vecPosition = XMVectorSet(rSet.fX, rSet.fY, engine::gBaseHeight.Get(), 1.0f)`.
     Reading that tweak here matches the spawn path at `Players.cpp:293`.
   - Treat the value as untrusted, because on the client it arrives over the
     wire: skip the write with a `kNetwork` `kWarning` when either float is
     non-finite or when
     `engine::IsOutOfBounds(engine::ComputeFrameBounds(rStaticData.vecArea), vecPosition)`
     is true (`Engine/Source/Frame/FrameUtils.h:56,132`). Both tests are
     deterministic and identical on both builds, so a rejected placement is
     rejected on both sides and the CRC stays matched.
   - Otherwise assign `rCurrentInterpolate.pVecPositions[iIndex] = vecPosition`.
     Write nothing else: velocity, direction, navigation state, flags, and
     timers stay as they were. `pVecPositions` is a `SharedCrcMembers()` column
     (`Frame/Collections/Players/Players.h:128-130`), which is why this is a
     deliberate CRC-affecting write; writing committed interpolate position
     from a fixed-tick phase already happens at `Players.cpp:427`
     (`PlayersPostRender::Spawn`).

   The player keeps its velocity, so from the next tick it drifts away from the
   placed point at its normal speed. That is intended: the pusher scenario
   needs a chosen separation, not a frozen unit, and freezing would need extra
   state this Plan does not add.
5. Harness surface, in `BuildInjectedChange`
   (`AgentCommandsServer.cpp:763-808`): accept `type == "SetPlayerPosition"`,
   requiring the existing `playerUuid` parameter plus `pos`, an array of
   exactly two JSON numbers that are finite after conversion to `float`; any
   other shape throws, which the transport turns into the ordinary
   `ok:false`/`error` response with no state change. Add the type to the
   `'type' must be ...` error string. The command's own response shape,
   guards, and `deferred` semantics are unchanged.

Determinism: the built change is queued through
`QueueAgentStatusChange` like every other injected change, so it reaches the
per-tick `FrameInput` the server publishes to subscribed clients and records in
the replay difference stream. Client and server therefore apply the identical
write during the same tick's Update phase, before the shared CRC is stamped.

Documentation for this item: the `inject_status_changes` schema bullet in
`Projects/BrokenEngineSandbox/Documents/AgentHarness.md` gains the new type and
its parameters, and that file gains a short placement recipe under
"Authoritative verification" showing two players separated inside one cell.
`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` names which status
changes are consumed in Update and states the current `FrameInput::kiVersion`,
so both statements are updated. `.agents/skills/agent-harness/references/command-reference.md`
owns only the five engine-shared commands and needs no edit.

### 2. One warning line at the pusher zone-overflow site

In `PushersInterpolate::SetupZones`
(`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:43-95`), count the
dropped registrations in a local `int64_t` at the existing cap check
(`PushersUpdate.cpp:85-89`, keeping `DEBUG_BREAK()` and `continue`), and after
the registration loop emit one line when that count is non-zero:

```cpp
LOG(kDefault, kWarning, "PushersInterpolate::SetupZones dropped {} pusher zone registrations (cap {} per zone)", iDroppedRegistrations, kiMaxPushersPerZone);
```

Decisions behind that exact shape:

- One summary line per `SetupZones` call, not one per drop. At the densities
  that reach the cap a per-drop line would emit hundreds of lines per tick per
  cell and flush the agent-readable log ring, hiding the very signal it is
  meant to give.
- `kDefault` category: there is no simulation or physics category
  (`Common/Log/LogTypes.h:18-31`), and this is a durable message, so `kTemp` is
  wrong. `kWarning` is the root level for "investigate; may spam".
- Only integers are formatted, so the site stays allocation-free as the
  allocation-tracked main loop requires; `/repo-code-review` owns the accepted
  formatting wrappers.
- No cell coordinate in the message: `SetupZones` receives only the frame, and
  the cell's `FrameStaticData` is not reachable from it today. The pusher
  coverage Plan will pass `rStaticData.vecArea` in, but adding a parameter here
  purely for the message is work this Plan does not need.

`Engine/Source/Frame/Collections/Pushers/AGENTS.md` states the cap as
"cap 512 per zone (overflow `DEBUG_BREAK`)"; that bullet is updated to say the
overflow also logs one `kWarning` summary per `SetupZones` call.

Note for the successor Plan: `Documents/Plans/Engine/PusherCellWideCoverage.md`
rewrites the surrounding bounds and explicitly keeps the cap and its overflow
handling. This counter and log line are part of that handling and must survive
that rewrite unchanged in meaning.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:7-20,36-52,182-200` —
  enum, name table, new payload struct, variant, `DefaultDataForType`.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:11` — `kiVersion`.
- `Engine/Source/Network/NetworkSerialization.cpp:144-241,293-402` —
  per-type wire size and the serialize/deserialize switches.
- `Engine/Source/Network/NetworkProtocol.h:63` — `kuiProtocolVersion`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:312-370`
  — `ProcessUpdateStatusChanges`, where the new branch goes; `:290-294` and
  `:504` are the spawn point and the base-height clamp this design cites, both
  read-only here.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:763-808` —
  `BuildInjectedChange`.
- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:43-95` — the
  registration loop and cap check.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the
  `inject_status_changes` schema bullet and the verification recipes.
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` — consumed status
  changes and the stated `FrameInput::kiVersion`.
- `Engine/Source/Frame/Collections/Pushers/AGENTS.md` — the zone-acceleration
  bullet's overflow wording.

## In scope

- Append `StatusChangeType::kSetPlayerPosition`, its `StatusChangeTypeName`
  case, the `SetPlayerPositionData` struct, its `StatusChangeData` variant arm,
  and its `DefaultDataForType` case in
  `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`.
- Add that type to `StatusChangeItemWireSize`, `SerializeGroup`, and
  `DeserializeStatusChangeBatch` in
  `Engine/Source/Network/NetworkSerialization.cpp`.
- Bump `FrameInput::kiVersion` to 16 and `engine::kuiProtocolVersion` to 12.
- Add the `kSetPlayerPosition` branch to
  `PlayersPostRender::ProcessUpdateStatusChanges`, including its id lookup, the
  finite and in-cell-bounds rejection with a warning, and the single write to
  `rCurrentInterpolate.pVecPositions`.
- Accept `type == "SetPlayerPosition"` with `playerUuid` and a two-number
  finite `pos` in `BuildInjectedChange`, and extend that function's type error
  message.
- Count dropped per-zone registrations in `PushersInterpolate::SetupZones` and
  emit the single `kDefault`/`kWarning` summary line described above, keeping
  the existing `DEBUG_BREAK()` and `continue`.
- Update `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` (the
  `inject_status_changes` schema bullet plus one placement recipe),
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` (consumed status
  changes and the stated `FrameInput::kiVersion`), and the zone-acceleration
  bullet in `Engine/Source/Frame/Collections/Pushers/AGENTS.md`.
- Client and server compilation plus the runtime evidence in the acceptance
  criteria.

## Out of scope

- Any change to pusher bounds, arena constants, zone size, zone count, the
  512-entry cap, the `SpatialAnchor` hook, or the drop policy itself — all of
  that belongs to `Documents/Plans/Engine/PusherCellWideCoverage.md`.
- Exposing pusher, velocity, or accumulated-push values in any query response.
- Any new harness query command, any new top-level harness command, and any
  replay-, CRC-, or checksum-reporting command.
- Placing any entity other than a player: spaceships, blasters, missiles, and
  targets keep their existing spawn paths.
- Changing where players spawn, freezing or zeroing a placed player's velocity,
  suppressing its navigation, or otherwise altering player behavior after
  placement.
- `spawn_players`'s 256 cap, the injection guards (replay, spawn-wait, paused
  deferral), and every other existing command's schema or response shape.
- Backward compatibility for replays or saves recorded before the version
  bumps, cross-version protocol tolerance, runtime toggles, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. The trigger is the root `AGENTS.md`
wire/protocol and serialization surface, not merely tool behavior: the new
status-change type is serialized into the per-tick `FrameInput` that the server
publishes to clients and records into replays, so it requires the codec
changes, the `FrameInput::kiVersion` bump, and the `kuiProtocolVersion` bump
named above. It is also a determinism/CRC surface, because the placement writes
`PlayersInterpolate::pVecPositions`, a `SharedCrcMembers()` column. The pusher
log line on its own would be Tier 2; the whole change classifies at the higher
tier.

Invariants to preserve:

- Client and server apply the same placement on the same tick from the same
  published `FrameInput`, and the rejection tests (non-finite, out of cell
  bounds) are deterministic and identical on both builds, so per-tick shared
  CRCs continue to match and a recorded replay reproduces them.
- `StatusChangeType` stays append-only, and the wire-size table, both codec
  switches, `DefaultDataForType`, and the variant arm list stay mutually
  consistent.
- Position W stays `1.0f` and `z` stays `engine::gBaseHeight.Get()`, matching
  the repository vector invariant and the per-tick clamp in
  `PlayersInterpolate::Update`.
- The pusher log site stays allocation-free, keeps `DEBUG_BREAK()` and the
  `continue`, and does not change which registrations are accepted or dropped,
  so pusher forces and CRCs are unchanged by item 2.
- No main-loop heap allocation is introduced by either item. In particular, the
  new overflow log line stays allocation-free under the repository LOG
  formatting rules. Allocation tracking has no runtime log signal and its
  `DEBUG_BREAK()` is a no-op without a debugger attached, so this invariant is
  established by code review, not by an acceptance run.

## Coordination

`Documents/Plans/Engine/PusherCellWideCoverage.md` depends on this Plan through
its `dependsOn` metadata: its acceptance criteria need both capabilities added
here. That Plan rewrites the pusher zone bounds around the overflow site and
must keep the drop counter and warning line added by item 2.

## Acceptance criteria

- The client and server both build.
- With a reset server and one active coord, `spawn_players {"coord":[0,0],"count":2}`
  returns two `globalIds`. `query_players {"coord":[0,0]}` then reports each
  player's `uuid` alongside its `globalId`, so each spawned player is selected
  by matching a returned `globalId` to that response's entry and taking its
  `uuid`; `spawn_players` itself returns no `uuid`. Two
  `inject_status_changes` `SetPlayerPosition` changes — one per matched
  `playerUuid`, targeting two points about 600 m apart inside that cell —
  then make a second `query_players {"coord":[0,0]}` report each player within
  25 m of its requested point and the two players more than 400 m apart. The
  tolerance covers the drift of a player that keeps its velocity between the
  placement tick and the query.
- A rejected placement changes nothing: a `pos` that is not two finite numbers
  returns `ok:false` with an error and leaves both reported positions
  unchanged, and a `pos` outside the cell's bounds returns `ok:true` while
  `query_players` shows the target player's position unchanged and the server
  log carries the new out-of-bounds warning.
- Over a run containing the placements, with a connected client, the server and
  client logs carry no newly appended `LogDifferences CRC Client`,
  checksum-mismatch, or `CONFIRMED DESYNC` line, proving client and server CRCs
  still match across a placement.
- A replay recorded across the placements and played back per the replay
  determinism recipe in `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`
  reaches its `End replay <tick>, looping` marker with no newly appended
  replay-reader, checksum, or CRC error line.
- The new overflow warning appears when the cap is exceeded: three
  `spawn_players {"coord":[0,0],"count":256}` calls put 768 players on the
  single fixed spawn point, and server `get_logs {"pattern":"SetupZones"}`
  then returns at least one line reporting a non-zero dropped-registration
  count.
- The same query returns no such line in a control run where a single
  `spawn_players {"coord":[0,0],"count":256}` call keeps every zone under the
  512-entry cap.

## Notes

The overflow criterion deliberately uses the existing fixed spawn point rather
than the new placement command, so the two items can be verified
independently.
