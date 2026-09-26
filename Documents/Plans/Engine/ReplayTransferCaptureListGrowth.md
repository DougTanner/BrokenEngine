<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T19:49:10.791Z","dependsOn":[]} -->
# Stop the replay transfer capture list from breaking on ordinary recordings

## Context

`engine::ReplayFixtures::ObserveAcceptedTransfers`
(`Engine/Source/Agent/Commands/ReplayFixtures.cpp:280-301`) appends one
`TransferCaptureEvent` for each distinct recording tick that has accepted
transfers. `RecordingStarted` (`:78-87`) reserves `kiReservedEvents` = 16
entries (`Engine/Source/Agent/Commands/ReplayFixtures.h:35`). When the list is
full, the next new tick logs
`kError` "Replay transfer capture list under-sized" and calls `DEBUG_BREAK()`
(`ReplayFixtures.cpp:291-295`), then pushes anyway.

The capture runs for every server recording, not only fixture runs:
`Replay::Replay` attaches the binding unconditionally
(`Engine/Source/File/Replay.cpp:263`), and `Replay::CaptureAcceptedTransfers`
calls `ObserveAcceptedTransfers` for every accepted transfer batch
(`Replay.cpp:331-362`). The number of transfer ticks in a recording depends on
game traffic and has no limit, so a recording with natural traffic can go past
16 ticks.

This was observed during an `/agent-harness` `replay_record` with a
`replay_transfer_fixture` player transfer [0,0]→[1,0] on a server with 8
players and many blasters. The recording started at tick 1029. At tick 1324 the
server logged "Replay transfer capture list under-sized Tick: 1324 Size: 16
Capacity: 16" and reached `DEBUG_BREAK` at `ReplayFixtures.cpp:294`. Recording
stopped at 1495, and playback completed. `Temp/server-agent.log` lines 16-39
hold the evidence (session-local, not durable). The break is a false alarm:
the push past the reserve is correct, and the comment at `:296-297` states that
`ServerTransferManager::HarvestTransfers`' allocation-tracking suppression
(`Engine/Source/Network/Server/ServerTransferManager.cpp:303`) covers it.

## Design

Author's recommendation: remove the `LOG` + `DEBUG_BREAK()` capacity check from
`ObserveAcceptedTransfers`, along with the part of the heap comment that refers
to it. Keep the reserve in `RecordingStarted` as a pre-size. Growth past it is
legitimate and already covered by allocation-tracking suppression. Raising
`kiReservedEvents` alone would only move the threshold, because recording
length has no limit.

## Critical files

- `Engine/Source/Agent/Commands/ReplayFixtures.cpp` — `ObserveAcceptedTransfers`, `RecordingStarted` comment.
- `Engine/Source/Agent/Commands/ReplayFixtures.h` — `kiReservedEvents`.

## In scope

- The capacity check (`LOG` + `DEBUG_BREAK()`) and its heap comment in
  `engine::ReplayFixtures::ObserveAcceptedTransfers`.
- The `RecordingStarted` reserve comment, only if its wording depends on the
  removed check.

## Out of scope

- Which events are captured, the `TransferCaptureEvent` shape,
  `replay_transfer_capture` JSON output, and `replay_transfer_fixture`.
- `Replay::CaptureAcceptedTransfers`, replay file formats, and
  allocation-tracking scopes.

## Risk

Change Workflow Tier 2. Trigger: one subsystem's runtime behavior (the server
agent/replay-fixture diagnostic). There is no determinism/CRC, wire,
serialization, or threading exposure, because the capture list is not in
frame state.

## Acceptance criteria

- An `/agent-harness` `replay_record` with natural transfers on more than 16
  distinct ticks logs no "Replay transfer capture list under-sized" error and
  reaches no `DEBUG_BREAK`, and `replay_transfer_capture` still reports every
  event.

## Notes

- `Documents/Investigations/JevSessionResidueJudgment.md` also cites
  `ReplayFixtures.cpp:293-294` as a log and break pair that "reads as
  permanent". This Plan removes that pair. That Investigation uses it only as
  scanner evidence and does not depend on it.
