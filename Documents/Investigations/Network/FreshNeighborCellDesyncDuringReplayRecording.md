# Confirmed client desync in a freshly subscribed neighbor cell during replay recording

Findings record. Not decision-complete: the mechanism is unknown, so there is no
decided fix and this is not yet an executable Plan. It earns a Plan under
`../../Plans/Network/` once `/external-diagnose-bug` proves the root cause.

## Observation

During `/agent-harness` acceptance for the `OwnedEntityRegistryToEngine` change
(2026-08-15), the first replay-recording run produced three client-side
confirmed desyncs in cell `(2,0)`, escalating to a forced disconnect. The run did
not reproduce afterwards.

The originating logs live under `Temp/`, which is untracked and transient, so the
decisive lines are quoted here verbatim.

Client (`Temp/client-agent-3.log:107-125`):

```
[Tick: 15836] CONFIRMED DESYNC after full rollback/replay Coord: (2,0) DesyncTick: 15835 ExpectedCrc: 0xED96D3A8FA2CD95C ActualCrc: 0x426D8A3FE6B725AB ReplayTicks: 2 NewConfirmed: -1
[Tick: 15836] Client::SendDesyncReport Frame: 15835 Grid: (2,0) Expected: 0xED96D3A8FA2CD95C Actual: 0x426D8A3FE6B725AB
[Tick: 15836] ClientDesyncManager::RecoverFromDesync DesyncCount: 1 / 3
[Tick: 15836] Client::SendResyncRequest
[Tick: 15848] CONFIRMED DESYNC after full rollback/replay Coord: (2,0) DesyncTick: 15848 ExpectedCrc: 0xE11510C99CCEAD70 ActualCrc: 0xFEC925B5CB7882B4 ReplayTicks: 2 NewConfirmed: -1
[Tick: 15848] ClientDesyncManager::RecoverFromDesync DesyncCount: 2 / 3
[Tick: 15858] CONFIRMED DESYNC after full rollback/replay Coord: (2,0) DesyncTick: 15858 ExpectedCrc: 0xCA9892FEAD7BFEBF ActualCrc: 0xF683022D7702A274 ReplayTicks: 2 NewConfirmed: -1
[Tick: 15858] ClientDesyncManager::RecoverFromDesync DesyncCount: 3 / 3
[Tick: 15858] ClientDesyncManager::RecoverFromDesync Escalating to disconnect
[Tick: 15858] ENET_EVENT_TYPE_DISCONNECT
```

Server (`Temp/server-agent.log`), in tick order across the same window:

```
[Tick: 15668] Recording started for 2 coords
[Tick: 15668] Server::ClientSubscribe Client: 3 Coord: (1,0) Slot: 0
[Tick: 15668] Server::ClientSubscribe Client: 3 Coord: (0,0) Slot: 1
[Tick: 15668] Server::ClientSubscribe Client: 3 Coord: (2,0) Slot: 2
[Tick: 15732] Server::ClientUnsubscribe Client: 3 Slot: 1 Coord: (0,0)
[Tick: 15784] Server::ClientSubscribe Client: 3 Coord: (2,0) Slot: 0
[Tick: 15785] Server::SendCoordStaticData Client: 3 Slot: 0 Coord: (2,0)
[Tick: 15785] Server::SendCoordFullState Client: 3 Frame: 15785 Slot: 0 Coord: (2,0)
[Tick: 15832] Server::ClientUnsubscribe Client: 3 Slot: 2 Coord: (2,0)
[Tick: 15832] Server::ClientSubscribe Client: 3 Coord: (2,0) Slot: 1
[Tick: 15833] Server::SendCoordStaticData Client: 3 Slot: 1 Coord: (2,0)
[Tick: 15833] Server::SendCoordFullState Client: 3 Frame: 15833 Slot: 1 Coord: (2,0)
[Tick: 15841] Server::ClientDesyncReport Frame: 15835 Grid: (2,0) Expected: 0xED96D3A8FA2CD95C Actual: 0x426D8A3FE6B725AB
[Tick: 15842] Server::SendCoordFullState Client: 3 Frame: 15842 Slot: 1 Coord: (2,0)
[Tick: 15853] Server::SendCoordFullState Client: 3 Frame: 15853 Slot: 1 Coord: (2,0)
[Tick: 15955] ServerSession::SendPlayerState State: ChangedFrame Client: 3 GlobalPlayer: 1 Grid: (2,0)
[Tick: 16060] Recording stopped
```

Shape of the situation: recording started at tick 15668 for two coords, in the
middle of a fleet migration, and a third cell became active inside the recording
window. Cell `(2,0)` was subscribed at 15784 as a visible neighbor holding no
owned entity — the first owned entity entered it at 15955, after the first
desync. Three ticks before the first desync the server moved the already
subscribed `(2,0)` from slot 2 to slot 1 (unsubscribe then resubscribe at 15832)
and re-sent static data plus a full state at 15833. That coincidence is a lead,
not a proven cause.

## Why this is pre-existing, not a defect of the session change

- `(2,0)` had no owned entity at any of the three desync ticks; the first arrived
  at tick 15955.
- The state the session moved (`OwnedEntity` registry) is transport and
  authorization state that is outside the PostRender CRC.
- `/repo-code-review` and `/adversarial-review` both found statement-for-statement
  behavioral equivalence with the baseline.
- Two targeted repro attempts failed: a driven `UpdateFleet` cell crossing with
  recording active for roughly 9,800 ticks, and the clean standard replay
  acceptance run.

## Open questions for diagnosis

1. Does a cell that becomes active *inside* a recording window get recorded from
   a consistent starting state, or can the recording begin mid-cell-activation?
2. Does the slot reassignment of an already subscribed coord (unsubscribe and
   resubscribe of `(2,0)` at 15832) change what the client rolls back and replays
   for that cell?
3. Is a freshly subscribed neighbor cell holding no owned entity replayed from
   the same inputs the server used, given its full state arrived mid-window?

## Suggested route

`/external-diagnose-bug` owns the mechanism. A run of the same scenario against a
baseline build (before the `OwnedEntityRegistryToEngine` change) would settle
pre-existing definitively rather than by the equivalence argument above. Because
the failure did not reproduce in two attempts, diagnosis should expect to need a
seeded or forced reproduction of the exact ordering above — recording start
during a migration, a third cell activating inside the window, and a slot
reassignment of an already subscribed coord — rather than a repeat of the plain
acceptance run.

## Origin

Accepted pre-existing/out-of-scope residual from the `OwnedEntityRegistryToEngine`
session (session branch `claude/cb6e15ef-fb81-4c19-957d-fd0ecbfeb5a5`). The
`Temp/` logs quoted above are machine-local and transient.
