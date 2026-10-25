<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-25T15:43:50.959Z","dependsOn":[]} -->
# Keep the client simulation ceiling active after a load reset while the server is paused

## Context

A Debug `/agent-harness` run with the server paused tripped the client's
render regression guard, which logs an error and calls `DEBUG_BREAK`. The guard
is the `InterpolateFrame` lambda in `GameBase::UpdateRenderInterpolation`
(`Engine/Source/GameBase.cpp:806-813`). The run went like this:

- The harness `pause {"paused":true}` paused the server at tick 3961.
- Three `edit_frame` calls followed. Each one ran the load-time
  resynchronization. The server answered each with new subscriptions and full
  states at Frame 3961.
- While the server stayed at 3961, the client's `describe_scene.tick` read
  4083 to 4119.
- After `pause {"paused":false}`, the server reported tick 3964. The client
  logged `Clock snap OldTick: 4090 NewTick: 3958 LatestServerTick: 3962
  TargetBehind: 4`, then
  `Render regressed to older frame Coord: (1,0) Tick: 3982 LastTick: 4086`
  and the `DEBUG_BREAK` line for `GameBase.cpp:811`.
- At server tick 3984, the owned omni moved into the subscribed neighbour
  (1,0) (`ChangedCell ... Grid: (1,0)`).

The run that found this was an acceptance check of the hexagonal-cell change.
That check passed. This failure comes from code the change does not touch:
apart from coordinate field names in two log lines, the hex change leaves every
function below as it was.

Verified cause, in order:

1. Each load notification calls `ClientSessionRuntime::ResetForServerLoad`
   (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:176-200`). Its
   `ResetClock` (`:86-98`) sets `miLatestServerTick = -1`.
   `ClientSessionBase::ResetSessionForServerLoad`
   (`Engine/Source/Network/Client/ClientSessionBase.cpp:98-117`) then zeroes
   `miTickCounter` and clears every cell.
2. `ApplyReceivedFullStates` (`ClientSessionRuntime.cpp:285-311`) adopts the
   first full state. It sets `miTickCounter` to the full-state tick minus the
   jitter floor, but it never sets `miLatestServerTick`. Only the delta path in
   `ApplyReceivedUpdates` (`:365-375`) restores that value. A paused server
   sends no deltas.
3. With `miLatestServerTick < 0`, two things follow:
   - `EstimatedLatestServerTick` (`:452-462`) returns -1, so the client
     simulation ceiling in `GameBase::ClientUpdate`
     (`Engine/Source/GameBase.cpp:306-319`) is `-1` and does not clamp.
   - `EvaluateClock` (`ClientSessionRuntime.cpp:765-771`) returns no
     correction.

   So the unpaused client simulates about 128 speculative ticks past the
   paused server.
4. The server's pause never reaches the client. The harness `pause`
   (`Engine/Source/Agent/AgentCommandsServerGeneric.cpp:110-119`) and
   `kClientPauseRequest` (`Engine/Source/Network/Server/ServerSessionBase.cpp:170-176`)
   set only the server's `GameFlags::kPaused`, and no server-to-client pause
   packet exists (`Engine/Source/Network/NetworkProtocol.h`). The client pause
   key (`GameBase.cpp:153-164`) stops only the requesting client's own
   simulation.
5. On unpause, the first delta restores `miLatestServerTick`. The clock error
   is then far above `kiClockSnapThreshold` (28), so `ApplyClockCorrection`
   (`ClientSessionRuntime.cpp:856-868`) snaps `miTickCounter` back about 132
   ticks. The speculative frames stay in the ring.
6. The server-only transfer into (1,0) fails CRC against the speculative
   frames. Reconcile rolls back and replays only up to the snapped-back tick
   counter, which cuts the ring's head back. The cell's `iLastRenderedTick`
   still holds the speculative 4086. Only `Cell::ResetClientState`
   (`Engine/Source/GameBase.h:137-142`) resets it, and neither a clock snap
   nor a rollback calls that. So the guard trips.

When the server is paused without a load reset, the clock reference survives.
The ceiling then holds the client within `kiMaximumEstimatedServerTickLead`
(16) plus `kiSimulationCeilingSlackTicks` (3) of the last server tick, as
`Documents/Architecture/Network.md` `## Client Global` hard-ceiling bullet
describes for a debug pause. Only a load reset with no delta after it removes
that bound.

How to reach it. Normal play cannot reach it. These debug paths can, each with
the server already paused or paused by the load:

- the harness `pause` followed by `edit_frame`;
- the harness `load` with `pauseAfterLoad: true`.
  `ServerSaveLoad::ServerLoad` calls `GameBase::ResetSession`, which clears
  `kPaused` (`Engine/Source/GameBase.cpp:1118`), and then `CommandLoad` sets it
  again (`AgentCommandsServerGeneric.cpp:160-163`).

A plain debug load always unpauses the server, so the server sends deltas at
once.

## Design

The fix session decides how the client behaves across a load reset while the
server is paused. The evidence shows the defect but does not settle the right
behavior. The author recommends the first option below; the rest were
considered.

1. Recommended: when the adopted full state sets the game clock, also treat its
   tick as the clock reference. In the first-adoption branch of
   `ApplyReceivedFullStates`, while `miLatestServerTick < 0`, set
   `miLatestServerTick` and `mLatestServerTickArrival` from the adopted
   full-state tick, as the delta path does. Rationale:
   - A new subscription's full state is sent at the server's current tick, so
     it is a real server-tick observation.
   - The post-load state then matches the ordinary paused-server case, which
     the existing ceiling and lead cap already bound by design.
   - The client stays within about 15 ticks of the paused server, so the error
     on unpause stays below the snap threshold and no backward snap happens.
   - The change touches only client clock state, with no wire or CRC change.
2. Considered: tell clients about the server's pause state, so the client
   pauses too. This needs a new engine packet, a `kiProtocolVersion` bump, and
   pause semantics across several clients. It makes the change Tier 3.
3. Considered and not recommended: reset `iLastRenderedTick` on a backward
   clock snap or a ring cut. This hides the symptom. The guard reports a
   genuine render of an older frame, and the free-run would remain.

## Critical files

- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`:
  `ApplyReceivedFullStates`, `ApplyReceivedUpdates` (reference update, read
  only), `ResetClock`, `EstimatedLatestServerTick`, `EvaluateClock`,
  `ApplyClockCorrection`.
- `Engine/Source/GameBase.cpp`: `ClientUpdate` simulation ceiling and
  `UpdateRenderInterpolation` regression guard (read only).
- `Engine/Source/Network/Client/AGENTS.md` `## Transport Timing Inputs` and
  `Documents/Architecture/Network.md` `## Client Global`, if the clock
  reference's sources change.

## In scope

- The first-adoption branch of `ClientSessionRuntime::ApplyReceivedFullStates`.
  It seeds the clock reference when none exists (recommended design).
- The `Engine/Source/Network/Client/AGENTS.md` `## Transport Timing Inputs`
  sentences and the `Documents/Architecture/Network.md` `## Client Global`
  wording about what sets `latestServerTick`, only where that change makes
  them incorrect.

## Out of scope

- The render regression guard in `GameBase::UpdateRenderInterpolation` and
  `Cell::ResetClientState`.
- The `EvaluateClock` correction and the `ApplyClockCorrection` snap threshold
  and target.
- Server pause handling and any server-to-client pause packet, unless the fix
  session picks Design option 2.
- `ResetForServerLoad`, `ResetSessionForServerLoad`, and the resync resets in
  `ClientSessionBase` (`ResetCoordinateStatesForResynchronization`,
  `OnRuntimeDisconnected`).
- The pending-full-state branch of `ApplyReceivedFullStates`.

## Change Workflow tier

Tier 2. Trigger: scoped runtime behavior in one subsystem, Engine Network
Client clock pacing. The recommended design changes no CRC'd state, wire
format, save, replay, or threading. Which ticks the client simulates
speculatively may change. Their PostRender results stay deterministic, and
mismatches still resolve through reconcile. Picking Design option 2 makes the
change Tier 3 (wire/protocol).

## Acceptance criteria

Run in a Debug client and server under `/agent-harness`. Setup, as the
observing run recorded it:

1. Connect a client.
2. In `FleetPanel`, click `[+O]##Omni`, then click the new owned omni's member
   item to focus it.
3. Send `wheel {"notches":4}`.
4. On the server, send `pause {"paused":true}`.
5. Send `edit_frame` on coord `[0,0]` for the owned omni's `omnis` row with
   `pVecPositions` `[0,0,6]` and `pVecVelocities` `[0,0,0]`. Repeat with
   `[0,440,6]`, then with `[381.05,220,6]`, which is next to the (1,0) edge
   with (1,0) subscribed.
6. Send `pause {"paused":false}`.

- After any paused `edit_frame` and a 4 s wait, the client
  `describe_scene.tick` minus the server `status.tick` is below 28. Before the
  fix it was about 120 to 155.
- After unpausing, and once the omni has changed cell to (1,0) (server log
  `ChangedCell ... Grid: (1,0)`), client `get_logs` holds no
  `Render regressed` line and no `Clock snap` line with `NewTick` below
  `OldTick`, and the client is still running.
- With the server unpaused throughout, the same `edit_frame` steps still
  resubscribe and the client keeps simulating with no clock snap.

## Notes

- Live verification is required. The defect needs a running client and a
  paused server, so a diff alone cannot settle it.
- `DEBUG_BREAK` (`Common/ErrorUtils.h`) always logs its line but breaks only
  when `kbDebugBreak` is on and a debugger is attached. Without a debugger the
  client renders the older frame and keeps running, so the
  `Render regressed` log line is the check.
