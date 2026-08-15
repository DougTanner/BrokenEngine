# Live client desyncs while the server plays back a recorded replay

Findings record from active-set harness verification (2026-08-15). Not
decision-complete: the root cause is unknown and no fix is decided, so this is
not an executable Plan. Route it through `/external-diagnose-bug`; promote it to
`Documents/Plans/<area>/` once a proven root cause and a decided fix exist.

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

## What is not established

- Whether it is pre-existing. No pre-change replay baseline exists; the baseline
  run never exercised recording or playback, so this cannot be attributed to, or
  cleared of, the active-set skeleton move.
- Why only coord `(-1,1)` desyncs, and whether the transient mid-recording
  activation of that coord is the trigger.
- Why the server's own per-tick replay CRC resimulation matched while the live
  client's CRC did not: candidate directions are what state a client receives for
  a coord that activates during playback rewind versus during normal simulation,
  and how the client's rollback window interacts with the server's replay rewind.

## Reproduction

Harness run A3 steps 1-11 as recorded in `Temp/activeset-after.md`, with drivers
`Temp/after-*.{ps1,json,log,txt}`. `Temp/` is untracked and may already be gone;
the shape is: set server log level `Debug`, baseline the
`[Rr]eplay|CRC|[Cc]hecksum|[Dd]esync` log pattern on both ports, start
`replay_record` while a non-origin coord is active and player-held, move the
fleet so that coord leaves the active set, stop recording, then `replay_play`
with the client still connected and watch client logs for `CONFIRMED DESYNC`.

## Boundary evidence

Outside the active change: `Documents/Plans/Network/ActiveSetSkeletonToEngine.md`
`## In scope` covers moving three active-set functions into the engine runtime
with unchanged ordering plus two game hooks, and explicitly puts `GameSaveLoad`
and replay-writer semantics, and client-side active-set handling, out of scope.
The failure is a client/server replay-playback desync, not a change in which
coords are active: the same run's active-set expectations passed.
