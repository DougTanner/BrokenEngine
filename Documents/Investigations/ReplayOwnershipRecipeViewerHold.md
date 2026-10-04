# Replay ownership recipe: Debug log markers and holding A during playback

Open question: how should the harness recipe
`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md`
`### Natural persistent-GUID ownership and cell crossing` keep coordinate A
subscribed through playback, when the client moves its own viewer without any
harness command? Not yet a Plan because the evidence does not decide between
holding A and relaxing step 10 (see `## Open decision`).

## Findings

Both came from an acceptance run of that recipe (run evidence was session
scratch, `Temp/subreq-harness-evidence.md` `## Check 7` with
`Temp/subreq-server.log`, and is not retained).

### 1. Playback markers need the server Default category at Debug (recommended)

- Step 8 retains the server-log slice from `ReadGrid F7.replay.grid` through
  `End replay <tick>, looping`. Both lines log at `kDefault, kDebug`:
  `Engine/Source/File/GridSave.cpp` (`ReadGrid {} iVersion: ...`, ~:161) and
  `Engine/Source/File/Replay.cpp` (`End replay {}, looping`, ~:1109).
- Step 1 raises only the Network category (`Verbose`, clamped to `Debug` on a
  Debug build), so neither marker emits. The run wrote no marker until the
  server Default level was set to Debug during the second playback loop.
- Recommended fix (author's recommendation, not yet user-confirmed): step 1
  also sends server `set_log_level {"category":"Default","level":"Debug"}`
  before the baselines. It does not depend on finding 2.

### 2. The client's auto-follow unsubscribes A during playback (open)

- Step 8 requires no parking or unsubscribe command during the first playback
  loop, and step 10 requires A to stay in `describe_scene.subscribedCoords`.
  The run sent no such command, yet the server logged
  `Server::ClientUnsubscribe Client: 1 Slot: 0 Coord: (1,0)` (A) at ticks 4259
  and 6846 in every loop, after the replayed A->B crossings at E1=3986 and
  E3=6458, and re-subscribed A at 5420 and 6915, before the B->A crossings at
  E2=5601 and E4=7109. So the restarts at E2/E4 were reached through a fresh
  subscribe and full state, not through the continuously held slot the
  scenario's intro says it must prove.
- Cause, from source: the replayed Player transfer reaches the client as a
  `PlayerEventType::kChangedFrame` for the client's own player, and
  `ClientSession::ApplyPlayerEvent`
  (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
  ~:131-137) calls `Game::SetClientGridCoordinate(rEvent.coordinate)` then
  `UpdateDesiredCoordinates`. The desired set becomes B plus B's visible neighbors
  (`ClientSession.cpp` `UpdateDesiredCoordinates`), and
  `ClientSessionRuntime::SynchronizeSubscriptions` unsubscribes A once its
  sticky window (`kStickySubscriptionDuration`, 2 s) expires.
  `FleetSelection::SynchronizeFleets` (`Projects/BrokenEngineSandbox/Source/FleetSelection.cpp`
  ~:241-261) also resets the viewer to the focused player's coordinate on every
  fleet sync.
- During recording the same auto-follow runs, which is why steps 5-7 park the
  viewer explicitly; the recipe assumes nothing moves the viewer during
  playback, and that assumption is false.

## Open decision

- Hold A by command: re-send client `set_client_grid_coord {"coord":A}` after
  each replayed crossing to B (E1, E3), within the sticky window. Unproven:
  whether a harness poll reliably lands inside the 2 s window, and whether a
  later fleet sync or `kChangedFrame` moves the viewer back to B before A's
  restart. Step 8's "no parking command" wording would need to allow
  re-centering on A.
- Relax step 10: accept that A is re-subscribed before E2/E4. This drops the
  continuously-held-slot restart path the scenario exists to prove, so the
  restart hold would need another route (for example the server-source proof
  step 10 already lists).
- A live run of the first option, checking `subscribedCoords` and the server
  unsubscribe log across E1..E4, decides between them.

## Scope when it becomes a Plan

`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md`
`### Natural persistent-GUID ownership and cell crossing` steps 1, 8, and 10
only; area `Game/`. No C++ change unless the decision chooses a client fixture
that suppresses auto-follow, which would raise the tier.
