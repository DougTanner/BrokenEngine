<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-11T23:43:48.946Z","dependsOn":[]} -->
# Pin the replay transfer capture snapshot to the armed fixture event

## Context

`replay_transfer_capture` reports one transfer event at a time, and an
unrelated organic transfer silently replaces the event a fixture created, so
the single-event acceptance requirement the harness replay documentation states
cannot be met on a populated cell.

Verified cause, from the current tree:

- `engine::ReplayFixtures::ObserveAcceptedTransfers`
  (`Engine/Source/Agent/Commands/ReplayFixtures.cpp:274-288`) resets
  `iRecordingEventTick`, `iPlaybackEventTick`, and `transferCounts` whenever an
  accepted transfer batch arrives on an event tick different from the latched
  one. Nothing distinguishes a fixture-queued event from an organic one, so the
  newest event always wins.
- `ObservePlaybackEvent` (`ReplayFixtures.cpp:290-296`) latches the tick of the
  most recently replayed event unconditionally, so the playback side is
  overwritten the same way.
- `replay_transfer_fixture`'s `pauseAfterWriterInput` isolates the recording
  side only: the arm is consumed at its writer input
  (`ReplayFixtures.cpp:255-267`) and nothing keeps the snapshot it produced
  alive after the harness unpauses.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:26` requires
  `recordingEventTick == playbackEventTick == E` for a single-event recording
  and calls the register lossy at line 51;
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:41`
  states the counts describe the last captured transfer event.

Observed in this session's server-only run, with a missile
`replay_transfer_fixture` from `[0,0]` to `[1,0]` and
`pauseAfterWriterInput: true` (`Temp/missile-transfer-attempt2.json`, a scratch
artifact, not tracked evidence):

- `captureAtEvent` at the automatic pause: `recordingEventTick` 3657,
  `missileCount` 1, all other counts 0, and the destination missile query
  showed `total` 1.
- `captureAfterUnpause`: `recordingEventTick` 3765, `blasterCount` 1,
  `missileCount` 0 — an organic blaster transfer had replaced the fixture event.
- `captureAfterPlayback`: `playbackEventTick` 3690 against that organic event,
  `missileCount` 0, and the destination missile query returned `total` 0.

The latch has behaved this way since the command existed, and the files below
are outside the `## In scope` boundary of every Plan this session was working
inside, so this is pre-existing, out-of-scope debt.

## Design

Recommended fix, because it removes the overwrite instead of working around it
and keeps the existing latch for callers that do not ask for isolation: extend
the isolation `pauseAfterWriterInput` already promises so it also covers the
snapshot, by pinning the fixture's event on both sides.

- When `ArmPauseAfterNextWriterInput` arms, mark the private `Binding`
  (`ReplayFixtures.cpp:15-22`) as awaiting a pinned event. The first event tick
  `ObserveAcceptedTransfers` latches while that mark is set becomes the pinned
  event; while a pin holds, a batch on a different event tick neither resets
  `iRecordingEventTick` nor clears `transferCounts`, and its counts are not
  added to the pinned event's counts.
- While a pin holds, `ObservePlaybackEvent` latches `iPlaybackEventTick` only
  for the pinned event tick, so `recordingEventTick == playbackEventTick == E`
  becomes observable.
- Clear the pin wherever the snapshot is already cleared — `Clear`, `Reset`,
  `RecordingInvalidated`, the recording start-failure and cancel paths, and
  `RecordingStarted` (`ReplayFixtures.cpp:32-99`) — and when a new arm replaces
  it. A fixture queued without `pauseAfterWriterInput` sets no pin and keeps
  today's last-event behavior.

The author recommends against the alternative of a `type` filter parameter on
`replay_transfer_capture`: the filter would run over a snapshot the organic
event has already overwritten, so it cannot recover the fixture's event, and it
would add a schema the command does not otherwise need.

Root-cause the pin's interaction with a multi-event recording before writing
code: `replay.md:35` records a multi-event matrix that polls per event, and that
matrix must keep working unchanged when no pin is armed.

## Critical files

- `Engine/Source/Agent/Commands/ReplayFixtures.cpp:15-22, 32-99, 246-296` —
  private `Binding`, the snapshot reset paths, arm consumption,
  `ObserveAcceptedTransfers`, and `ObservePlaybackEvent`.
- `Engine/Source/Agent/Commands/ReplayFixtures.h:28-58` — only if the pin
  changes the declared surface; the recommended pin lives in the private
  `Binding` and needs no header change.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:40-41`
  — the `replay_transfer_fixture` and `replay_transfer_capture` entries, whose
  last-event wording becomes wrong for a pinned fixture.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:26, 34, 51` —
  the latch description, scenario C, and the lossy-register caveat.
- `Engine/Source/Agent/AGENTS.md:14` — only if the `ReplayFixtures` server-slot
  description stops matching the observation behavior.

## In scope

- The pin state and its lifetime inside the private `Binding` in
  `Engine/Source/Agent/Commands/ReplayFixtures.cpp`, and the resulting
  conditions in `ObserveAcceptedTransfers`, `ObservePlaybackEvent`,
  `ArmPauseAfterNextWriterInput`, and the existing snapshot-clearing paths.
- Updating the harness command and replay documentation named above to state
  the pinned behavior and drop the wording it invalidates.

## Out of scope

- `replay_transfer_capture`'s response schema and `replay_transfer_fixture`'s
  request schema; no new parameter or result field.
- Production replay recording, playback, persistence, transfer harvest, sort,
  or capture behavior, including `Engine/Source/File/Replay.cpp` call sites.
- `game::CountCapturedReplayTransfers` and the game classifier contract.
- Any change to which transfers the simulation accepts or when.

## Risk tier and invariants

Expected Tier 2 (scoped behavior of one subsystem's tool surface). The highest
risk trigger is server-only, `kbDebugInput`-gated agent fixture observation
state: `ReplayFixtures` is compiled under `BT_SERVER` only and the command
refuses outside `kbDebugInput`, the snapshot is observation-only, and no
determinism/CRC, wire, serialization, `.pack`/`kiVersion`, save, or replay
payload surface is touched. Escalate to Tier 3 if the fix reaches
`Engine/Source/File/Replay.cpp` production paths or the command schema. The
fixture state stays outside the PostRender CRC, and the single server binding
stays main-thread only.

## Acceptance criteria

- With one fixture missile transfer queued at `pauseAfterWriterInput: true` on a
  cell that also produces organic transfers, `replay_transfer_capture` still
  reports that fixture event's `recordingEventTick` and `missileCount` 1 after
  the harness unpauses and after recording stops.
- During the resulting playback, a poll reports
  `playbackEventTick == recordingEventTick` for that fixture event, and the
  destination `query_collection` shows the restored missile row.
- A fixture queued without `pauseAfterWriterInput` and the multi-event matrix in
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:35` behave as
  documented today.
- The server `Debug|x64` build is clean, with no checksum, CRC, or
  `CONFIRMED DESYNC` line across a record-and-replay loop.

## Notes

- Live verification needs `kbDebugInput`, no active replay, a ready active
  source frame, distinct Chebyshev-adjacent coordinates, and a live destination
  for the missile type, per
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:40-41`.
- The scratch artifact named in `## Context` lives under `Temp/` and is not
  preserved; the cited cause is established from the source lines above.
