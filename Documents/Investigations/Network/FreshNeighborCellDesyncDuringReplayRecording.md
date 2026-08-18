# Confirmed client desync in a freshly subscribed neighbor cell during replay playback

Findings record. This report is now classified as a duplicate of the canonical
replay-transfer diagnosis in
[`Engine/ReplayPlaybackLiveClientDesync.md`](../Engine/ReplayPlaybackLiveClientDesync.md).
The earlier “pre-existing” attribution to the `OwnedEntityRegistryToEngine`
session is withdrawn; this file records the duplicate evidence and no longer
requests a separate Network diagnosis or Plan. The failure occurred during
playback after recording had stopped; the original log's simulation ticks are
reused during playback and are not wall-order timestamps.

## Observation

During `/agent-harness` acceptance for the `OwnedEntityRegistryToEngine` change
(2026-08-15), the first record-then-playback run produced three client-side
confirmed desyncs in cell `(2,0)`, escalating to a forced disconnect during
playback. The run did not reproduce afterwards.

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

Server (`Temp/server-agent.log`), in retained wall order (do not sort this
excerpt by its `[Tick: ...]` fields):

```
[Tick: 16060] Recording stopped
[Tick: 16060] ReadGrid F7.replay.grid iVersion: 200 iFrameCount: 2
[Tick: 16060] Game::Reset()
[Tick: 16060] ServerSession::ResetClientsForLoad
[Tick: 16060] Server::BroadcastLoadNotification
[Tick: 16060] ServerSession::ResetClientsForLoad Client: 3 no GUID match, will respawn
[Tick: 15668] ServerClientManager::NewClients Connecting Client: 3 Guid: (...) Empty: false
[Tick: 15668] Server::ClientSubscribe Client: 3 Coord: (2,0) Slot: 1
[Tick: 15685] Server::SendCoordStaticData Client: 3 Slot: 1 Coord: (2,0)
[Tick: 15685] Server::SendCoordFullState Client: 3 Frame: 15685 Slot: 1 Coord: (2,0)
[Tick: 15841] Server::ClientDesyncReport Frame: 15835 Grid: (2,0) Expected: 0xED96D3A8FA2CD95C Actual: 0x426D8A3FE6B725AB
```

The retained source slice makes the wall-order boundary explicit:
`Temp/server-agent.log:137` is the recording stop; `:138–147` are
`ReadGrid`/reset and load setup; `:148–162` are the playback rewind and
subscription/full-state work; and `:163+` contains the desync report. The
excerpt above keeps that source order;
the `15668`–`15841` tick values after reset are playback's reused simulation
ticks, not events that happened before the `16060` recording stop. The GUID is
redacted here because it is not needed for the timing conclusion.

On the recorded tick axis, recording started at 15668 for two coords, in the
middle of a fleet migration, and a third cell became active inside the recording
window. Cell `(2,0)` was subscribed at 15784 as a visible neighbor holding no
player; the first player entered it at 15955 on that recorded timeline. Three
ticks before the first reported desync tick, playback's reused tick axis shows
the already subscribed `(2,0)` moving from slot 2 to slot 1 (unsubscribe then
resubscribe at 15832) and receiving static data plus a full state at 15833. Those
tick relationships are leads only; the wall-order evidence establishes that the
desync report was emitted during playback after the recording stop.

## Duplicate resolution

The `(2,0)` run is the same replay transfer publication race documented by the
canonical investigation. The earlier excerpt was misleading because it sorted
the lines by simulation tick. The retained wall order proves the recording pass
ended first (`Temp/server-agent.log:137`), then `ReadGrid`/reset ran (`:138–147`),
playback rewound the tick labels while it rebuilt subscriptions (`:148–162`),
and the desync report followed (`:163+`). Thus this is a playback failure after
recording, not a recording-time failure:

- A coordinate entered the visible/recorded set during the run, and its
  post-dispatch transfer state belonged to event tick `E` while the replay input
  carrying that transfer belonged to `E + 1`.
- Before the fix, playback could omit the destination from the `E` dispatch and
  publication, then apply it one tick late. The server's own replay CRC path
  could remain self-consistent while the live client received a missing or late
  update and reported a confirmed desync.
- The unsubscribe/resubscribe slot change and the later first player at recorded
  tick 15955 are ordering coincidences, not the cause. The moved
  `OwnedEntityRegistry` remains outside the PostRender CRC and was behaviorally
  equivalent to the baseline; it is not implicated.

The prior conclusion that this was proven pre-existing is therefore stale and
withdrawn. The fix and acceptance contract are owned by
[`ReplayPlaybackLiveClientDesync.md`](../Engine/ReplayPlaybackLiveClientDesync.md):
`recordingEventTick == playbackEventTick == E`, where `playbackEventTick` is the
retimed tick latched during pre-dispatch successor discovery, and
`writerInputTick == E + 1`. Publication at `E` must be evidenced by the Network
Verbose `ServerBroadcaster::BuildTickPublication` line; first destination
dispatch at `E + 1` comes from the static phase trace, not a historical status or
query response. Require no replay or client CRC errors.

That `E + 1` retiming has since been superseded: the transfer is recorded at
event tick `E` in the difference stream's post-dispatch channel and playback
loads that exact tick (`FrameInput::kiVersion` 15), so `playbackEventTick` is a
direct observation and `writerInputTick` no longer exists. The surviving
acceptance contract is `recordingEventTick == playbackEventTick == E`, still
owned by the canonical investigation.

## Origin and status

Originally reported as an out-of-scope residual from the
`OwnedEntityRegistryToEngine` session (session branch
`claude/cb6e15ef-fb81-4c19-957d-fd0ecbfeb5a5`). The `Temp/` logs quoted above are
machine-local and transient. That provenance is retained for the original
observation; its pre-existing conclusion is superseded by the duplicate
resolution above. There is no separate Network follow-up Plan.
