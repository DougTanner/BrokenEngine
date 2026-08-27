<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:04.568Z","dependsOn":[]} -->
# Keep replay recording and playback mutually exclusive

## Context

The retained survivor `CAI/shard-0012/003` identifies an invalid replay mode
overlap. `CommandReplayRecord` admits `start:true` from
`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:291-321`
without checking active playback. `GameBase::ServerUpdate` enters
`Replay::SyncReplayTick` when `mbReplaying` or `kSaveReplay` is set at
`Engine/Source/GameBase.cpp:377-379`, while `Replay::SyncReplayTick` starts a
writer and invalidates the manifest at `Engine/Source/File/Replay.cpp:731-789`
without an `mbReplaying` guard. During the overlap,
`GameBase::FinalizeFrameTick` applies replay transfers instead of harvesting
live transfers (`Engine/Source/GameBase.cpp:511-521`), so a new recording cannot contain the
complete post-dispatch stream.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0012.md:70`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:594`. The frozen/live
source identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`;
no source was changed by this routing session. The shard traces the ordinary
`replay_transfer_fixture` path, so the gap is a pre-existing replay lifecycle
failure outside the audit work.

## Design

The author's recommendation is to reject a recording start while
`mbReplaying` or a load transition is active, before setting `kSaveReplay` or
invalidating the current manifest. Apply the reciprocal admission rule: reject
a playback/load start while `Replay::IsRecording()` is true or a recording
transition is pending, before setting `kLoadReplay` or staging readers. Apply
both checks to the agent handlers and the ordinary one-way game-packet handlers;
the F7/F8 client inputs use those same packets. Keep one owner for each replay
generation and preserve the existing post-dispatch transfer capture and
reader-retirement reload sequence; do not create a second mixed-mode format.

Keep a final main-thread execution-boundary guard in `GameBase` and `Replay`
around `SaveLoadReplay`/`SyncReplayTick`: no pending flag from any ingress may
be consumed while the opposite reader or writer owner is live, and the reader
and writer owners must never be true in the same tick. A rejected pending
transition is cleared through the existing flag/state machinery before any
reader, writer, or manifest mutation, leaving the active owner and manifest
intact. This boundary is the backstop for requests that arrive through a
different poll or are set by a path that bypasses a command handler.

Rejected agent transitions throw the existing handler validation exception so
`AgentCommandServer::Drain` returns its normal `{id, ok:false, error}` envelope;
successful `pending` results remain unchanged. The F7/F8 packet path is
one-way, so `ServerSession` must use the existing warning/error log and leave
the relevant replay flags and status unchanged rather than inventing a packet
response or configuration.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:282-337` — agent record/play transitions and their `pending` result behavior.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:75-85,203-214,244-251` — ordinary packet admission, replay request handlers, and existing one-way rejection logging boundary.
- `Projects/BrokenEngineSandbox/Source/Network/GamePacketType.h:17-24` — F7/F8 replay packets are one-way client-to-server debug controls.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:557-583` and `Engine/Source/Input/Input.cpp:59-70` — F7/F8 input mapping and packet send path.
- `Engine/Source/GameBase.cpp:305-321,365-427,511-521` — command/poll ordering, replay execution boundary, and transfer phase selection.
- `Engine/Source/File/Replay.cpp:411-721,723-789` — load/readers and writer-start/manifest lifecycles where the central owner guard must run before mutation.
- `Engine/Source/File/Replay.h:38-62,82-89` — `IsRecording`, replay owner state, and reader/writer lifecycle declarations.
- `Engine/Source/Agent/AgentCommandServer.cpp:332-395` — existing exception-to-agent failure envelope.
- `Engine/Source/File/AGENTS.md` — replay generation and transfer-channel
  invariants.

## In scope

- Admission and state transitions that allow recording and playback to overlap.
- Rejection/error publication before replay flags, reader/writer state, or
  manifest state mutate, in both directions: record-start during playback/load
  and playback/load while recording or a recording transition is pending.
- The shared execution-boundary guard in `GameBase`/`Replay` that prevents any
  ingress from combining reader and writer owners, including requests that
  arrive in different polls or through the F7/F8 packet path.
- Existing agent failure envelopes and one-way packet warning/error logging;
  no new packet response, configuration, replay mode, or wire format.
- The existing reader/writer retirement and post-dispatch transfer paths needed
  to keep each mode internally complete.

## Out of scope

- Replay file layout, `FrameInput`/`Frame` versions, transfer payload formats,
  or playback interpolation.
- New replay modes, recording-from-playback functionality, or compatibility
  readers.
- Unrelated agent command validation and save/load behavior outside replay mode
  admission.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change controls deterministic
replay state, save/reload compatibility, and post-dispatch transfer ordering.

Preserve these invariants:

- A tick has exactly one replay owner: live recording or playback, never both.
- A rejected transition leaves the active reader/writer and manifest intact.
- A pending `kSaveReplay` transition cannot be consumed with a live/pending
  playback load, and a pending `kLoadReplay` transition cannot be consumed with
  live/pending recording; this remains true regardless of ingress order.
- Valid recordings retain every live post-dispatch transfer and remain
  replayable with the current compatibility gates.

## Acceptance criteria

- `replay_record {"start":true}` issued during active playback is rejected
  without invalidating the loaded manifest or starting a writer; the same
  rejection applies when `kLoadReplay` is pending, and the active status remains
  `replaying:true`.
- The replay transfer fixture records and replays its post-dispatch transfer
  on the normal record-then-play path.
- `replay_play` issued while recording is active, including while a writer-start
  transition is pending, is rejected without setting `kLoadReplay`; recording
  remains active and no reader is staged.
- The equivalent ordinary packet sequence is covered for F7 during playback
  and F8 during recording; because those packets are one-way, the server log
  records the rejection and the existing status fields remain unchanged.
- Agent sequences cover both directions and a cross-ingress same-update case:
  `replay_play` then `replay_record {"start":true}`; recording then
  `replay_play`; and F7/F8 requests arriving around the opposite agent request.
  Every rejected command returns the normal agent error envelope, while the
  valid record-then-play sequence still reports its existing `pending` results.
- F7/F8 and agent transitions preserve the existing replay transfer fixture's
  post-dispatch capture and reader-retirement behavior when only one owner is
  active.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0012/003`; source selector is the shard line above and the
consolidated selector is `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:594`. No source fix or build
was performed during routing.
