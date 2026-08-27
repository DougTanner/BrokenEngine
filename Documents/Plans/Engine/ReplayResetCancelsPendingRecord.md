<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:15.358Z","dependsOn":[]} -->
# Cancel pending replay recording when load or reset replaces state

## Context

The accepted finding `CAI/shard-0044/002` identifies a replay lifecycle gap.
While paused, `replay_record {"start":true}` sets `GameFlags::kSaveReplay` but
no tick consumes it (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:291-321`).
`CommandLoad` and `CommandReset` then replace state through `ServerLoad` or
`ServerReset` (`AgentCommandsServer.cpp:249-280`).  `Game::Reset` clears replay
streams but clears only `kPaused` from `mGameFlags`
(`Projects/BrokenEngineSandbox/Source/Game.cpp:383-392,403`), so the next
unpaused `ServerUpdate` enters `SyncReplayTick` and starts a new writer from
the stale flag (`Engine/Source/GameBase.cpp:305-312,377-403`; `Engine/Source/File/Replay.cpp:731-795`).

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0044.md:74`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1056`.
The relevant source and authorities match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the stale flag is pre-existing,
unresolved, and outside the audit work.

Impact: a successful load or reset can silently create a replay generation on
the next tick even though the caller did not request recording after state
replacement.

## Design

Author's recommendation: cancel the pending `GameFlags::kSaveReplay` request
as part of the common state-replacement reset, alongside `Replay::ResetStreams`.
Leave active stream cleanup, pause handling, and the explicit
`replay_record` transition unchanged.  A new writer may start only after a new
explicit recording request against the replacement state.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:249-321` — load/reset and replay-record commands.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:383-413` — state reset and game flags.
- `Engine/Source/GameBase.cpp:305-312,377-403` — command/replay tick order.
- `Engine/Source/File/Replay.cpp:291-298,731-795` — stream reset and writer start.
- `Engine/Source/GameBase.h:54-62` — replay flag definitions.

## In scope

- Clearing or otherwise cancelling the pending replay-record flag during the
  existing `Game::Reset` state-replacement path.
- Preserving replay stream teardown, `pauseAfterLoad`, fresh-reset behavior,
  and the explicit later `replay_record` command.

## Out of scope

- Replay file format, writer generation, playback semantics, pause policy, or
  command schema changes.
- Active-recording error policy, unrelated replay flags, or a new replay
  lifecycle abstraction.
- Agent command validation not needed to preserve this reset ordering.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: state
replacement crosses the replay/save lifecycle and deterministic fixed-tick
processing boundary.

Preserve these invariants:

- Load/reset leaves no pending recording transition that can be consumed by a
  later tick without a new explicit request.
- Existing replay readers/writers are cleared before replacement state runs,
  and valid explicit recording requests still start one writer generation.
- Simulation state, frame CRC, replay format, and save compatibility remain
  unchanged.

Tier rationale: the fix clears one pending flag in the existing `Game::Reset`
teardown, next to the stream reset it already performs, and the Design leaves no
open decision. No replay format, tick ordering, or valid recording behavior
changes.

## Acceptance criteria

- While paused, arm `replay_record {"start":true}`, then perform `load` with
  both pause-after-load settings and `reset`; after unpausing, no writer starts
  and the recording status remains off.
- After each replacement, a fresh explicit `replay_record {"start":true}`
  starts recording normally and does not reuse the discarded stream.
- Existing active-recording and playback reset behavior remains unchanged.
- Server `Debug|x64` builds clean through `/compile`; the documented agent
  command sequence observes no hidden writer creation.

## Notes

The finding has no external claim or duplicate-family hint.  Its root is the
pending flag surviving the common reset, not stale profile counters or active
playback admission.
