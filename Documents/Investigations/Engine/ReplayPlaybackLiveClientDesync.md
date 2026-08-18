# Live client desyncs while the server plays back a recorded replay

Findings record from active-set harness verification (2026-08-15). The root
cause and fix are now proven. This is the canonical replay-transfer diagnosis;
the neighboring-cell finding
([`FreshNeighborCellDesyncDuringReplayRecording.md`](../Network/FreshNeighborCellDesyncDuringReplayRecording.md)) is a duplicate manifestation and no longer carries a separate attribution.

**Superseded mechanism.** The "Proven fix" below describes the original repair,
which kept recording the transfer in the `E + 1` `FrameInput` and undid that
delay at playback with a successor peek. The replay format has since changed:
the transfer is recorded at event tick `E` in the difference stream's
post-dispatch channel and playback loads that exact tick, so the `E + 1`
recording offset, the successor peek, and the retimed `playbackEventTick` no
longer exist (`FrameInput::kiVersion` 15). `playbackEventTick` is now a direct
observation, and `writerInputTick` is gone from the capture contract. The
diagnosis, observation, and reproduction below remain accurate history.

## Observation

Scenario (harness run A3): a live client stayed connected while the server
recorded a replay across a cell eviction and then played that replay back.
Recording started at server tick 5099 with `activeCoords` `[0,0] [0,1]`; the
fleet was moved to `[1,1]`, evicting non-origin coord `[0,1]` from the active set
at tick 6402; recording stopped at 6691; `replay_play` then ran to its recorded
endpoint.

The two stated expectations passed: the manifest `F7.replay.manifest` (v3,
`initialTick=4645`, `activationCount=4`, 18 inventory entries) was fully valid,
including all four file kinds for the evicted coord `[0,1]`, with matching sizes,
SHA-256s, and generation digest; playback reached `End replay 6608, looping`.
Server-side per-tick replay CRC resimulation reported nothing — no
`LogDifferences`, no checksum mismatch, no `SaveLoadReplay aborted`, no
`terminal data was not fully consumed`.

The live client nevertheless desynced during that playback:

```
[Tick: 4801] CONFIRMED DESYNC after full rollback/replay Coord: (-1,1) DesyncTick: 4800 ExpectedCrc: 0x85F2E4CE29C613BD ActualCrc: 0x1CFE8C4595FDDEDC ReplayTicks: 2 NewConfirmed: -1
[Tick: 4800] ClientDesyncManager::RecoverFromDesync DesyncCount: 1 / 3
[Tick: 4811] CONFIRMED DESYNC ... Coord: (-1,1) DesyncTick: 4811 ExpectedCrc: 0xEB8C585944FBFF29 ActualCrc: 0xB1933681A41CDBB3
[Tick: 4819] CONFIRMED DESYNC ... Coord: (-1,1) DesyncTick: 4819 ExpectedCrc: 0xCB17EF130682BE34 ActualCrc: 0x5F954D7BF071A1C0
[Tick: 4819] ClientDesyncManager::RecoverFromDesync DesyncCount: 3 / 3 -> Escalating to disconnect
```

The server logged the matching report:
`[Tick: 4807] Server::ClientDesyncReport Frame: 4800 Grid: (-1,1) Expected: 0x85F2E4CE29C613BD Actual: 0x1CFE8C4595FDDEDC`.
`clientCount` fell from 1 to 0 and the client returned to `kMainMenu` with a
disconnect dialog.

Every desync names coord `(-1,1)`, which is not part of the scripted movement: it
activated transiently at recorded tick 4797 (manifest `activation[2]`) and has a
small recorded footprint (header 740 B, frames 2443 B).

A control run in the same processes — single coord `[0,0]`, ~20 s recording, live
client connected across playback — produced zero new client desync lines and the
client stayed connected through a completed loop. So the failure is not "any live
client during any playback".

The retained server log is in wall order, not simulation-tick order: the recording
pass stops before the replay `ReadGrid`/reset and the later playback subscription
work. Playback reuses recorded tick values, so a desync line whose tick is inside
the recorded range must not be placed before the recording stop by sorting on its
`[Tick: ...]` field. The failure occurred during playback after recording had
finished.

## Canonical root cause

The transfer's simulation event and its replay input intentionally have
different ticks. `HarvestTransfers()` runs after dispatch at event tick `E`,
mutates the destination's next frame, and makes that destination state and CRC
part of the event-`E` publication. The replay writer stages the same transfer in
its next input, so the serialized `FrameInput` carries it at `E + 1`.

Playback previously loaded only the current difference and applied that
transfer before dispatch at `E + 1`. A destination that was activated by the
transfer was therefore absent from the `E` simulation dispatch and, in the old
path, from the active-coordinate publication list, even though the recording's
post-dispatch state belonged to `E`. The server's replay resimulation could
still validate its own delayed state, while a live client received a missing or
late authoritative update and failed rollback CRC validation. The same ordering
also allowed the engine per-coordinate resend ring to prune a publication-only
coordinate when pruning followed simulation membership rather than the update
coordinates actually supplied.

This is a replay transfer publication race, not a wire-format, client-starvation,
or transport-registry defect.

## Proven fix

- `DifferenceStreamReader::PeekNextDifference` is a const, nonadvancing peek. It
  observes the successor input without consuming the reader iterator and does
  not change the on-disk replay format.
- On playback, `GameSaveLoad` stages a transfer found in the `E + 1` successor
  input, dispatches the existing active set at `E`, then applies the staged
  transfer after dispatch and publishes its destination at `E`. The destination
  first enters simulation dispatch at `E + 1`; if `E + 1` is terminal, terminal
  validation retires it before another dispatch. The `E + 1` input is consumed
  without applying the transfer twice. Terminal input and the retained end frame
  are fully consumed and checksum-validated before a reader retires, and the
  current fixed-tick iteration stops before the next loop load.
- Live status/transfer batches are sorted into the deterministic type order
  before recording and publication. Replay uses the recorded `FrameInput` order
  verbatim.
- The publication path admits the transfer-only coordinate at `E` while the
  engine buffer prunes from the supplied update coordinates. This uses the
  existing per-coordinate packet/ring path and leaves wire layout, subscription
  starvation, and client timing rules unchanged.

`playbackEventTick` is a retimed event tick, not a direct observation: during
pre-dispatch playback at `E`, successor discovery sees the transfer in the
`E + 1` input and latches `E`. The acceptance contract is
`recordingEventTick == playbackEventTick == E`, `writerInputTick == E + 1`, and a
new Network-Verbose
`ServerBroadcaster::BuildTickPublication Coord: (...) Tick: E StatusChanges: ...`
line proving publication at `E`. The first destination simulation dispatch at
`E + 1` follows the static phase trace (successor peek/staging, normal `E`
dispatch, transfer apply/publication, then the next active-set dispatch); no
status or query response is historical timing evidence. Require no replay
checksum/read/terminal errors. The synthetic `replay_transfer_fixture` player
has an empty client GUID and proves state/CRC only; persistent ownership is
covered separately by the natural client-owned `UpdateFleet` cell-crossing
scenario in `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`.

## Reproduction

Harness run A3 steps 1-11 as recorded in `Temp/activeset-after.md`, with drivers
`Temp/after-*.{ps1,json,log,txt}`. `Temp/` is untracked and may already be gone;
the shape is: set server log level `Debug`, baseline the
`[Rr]eplay|CRC|[Cc]hecksum|[Dd]esync` log pattern on both ports, start
`replay_record` while a non-origin coord is active and player-held, move the
fleet so that coord leaves the active set, stop recording, then `replay_play`
with the client still connected and watch client logs for `CONFIRMED DESYNC`.

## Boundary evidence

The original active-set change in `Documents/Plans/Network/ActiveSetSkeletonToEngine.md`
covered moving three active-set functions into the engine runtime with unchanged
ordering plus two game hooks; it did not alter replay-writer semantics or the
client. That boundary remains useful evidence that the move did not create a
different active-set algorithm. The proven failure was the replay transfer
publication ordering described above, including the publication-only coordinate
at `E`; the stale “pre-existing” attribution in the duplicate neighboring-cell
finding is withdrawn.
