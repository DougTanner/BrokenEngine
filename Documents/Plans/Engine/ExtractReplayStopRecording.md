<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:26.633Z","dependsOn":[]} -->
# Move the stop-recording-and-publish branch out of SyncReplayTick into Replay::StopRecording

## Context

`Replay::SyncReplayTick` (`Engine/Source/File/Replay.cpp:712-1071`) handles four things per fixed tick: the record-start request, the record-stop request, writer updates, and playback reader retirement, activation, and loading. The record-stop branch (`Replay.cpp:784-916`) is a one-shot request: it clears `kSaveReplay`, saves every writer generation, writes metadata, builds and publishes the manifest, reports to the harness rig, logs, and returns `ReplayTickDecision::kDispatch`. It shares no locals with the per-tick branches that follow it. Its load-side counterpart, the playback request, already has its own member, `Replay::SaveLoadReplay`.

Origin: a read-only review of the top 20 functions by complexity mass at `43450112`. The user's governing rule, as relayed by the dispatching agent: a function is never split only because it is big; only a piece that is a separate job fused into its function moves. The record-stop branch was judged such a piece; the rest of `SyncReplayTick`, including the record-start branch, was judged earned and stays.

Verified at `43450112`: the branch body (`:786-914`) reads and writes only `mReplayWriters`, `miReplayInitialTick`, the member `UpdateTerminalReplayWriter`, `ReplayHarnessRigs` calls on `*this`, file-static helpers, and game globals (`game::gpGame`, `game::OnReplayStreamsInvalidated`, `game::WriteReplayMetadata`). Its only exit is the `return ReplayTickDecision::kDispatch;` at `:915`.

## Design

Author's recommendation, with rationale:

1. Declare `void StopRecording();` in the private section of `Replay` (`Engine/Source/File/Replay.h`), beside `UpdateTerminalReplayWriter`, because only `SyncReplayTick` calls it.
2. Define `Replay::StopRecording` in `Replay.cpp` directly before `Replay::SyncReplayTick`. Its body is `Replay.cpp:786-914` moved unchanged, including the `kFullFramesRecord` harness block at `:842-882`, whose relocation is a separate Plan (see `## Coordination`).
3. The branch in `SyncReplayTick` becomes `StopRecording(); return ReplayTickDecision::kDispatch;` under the unchanged condition at `:784`. The call stays inside the `if constexpr (kbDebugInput)` block, so no new guard is needed.

## Critical files

- `Engine/Source/File/Replay.cpp`
- `Engine/Source/File/Replay.h`

## In scope

- New private member `Replay::StopRecording`: its declaration in `Replay.h` and its definition in `Replay.cpp`.
- `Replay::SyncReplayTick`: replacing the record-stop branch body (`Replay.cpp:786-914` at `43450112`) with the call.

## Out of scope

- The record-start branch (`Replay.cpp:719-782`), the writer-update loop, and every playback branch of `SyncReplayTick`.
- Any change to the moved statements, their order, their `ReplayHarnessRigs` calls, or their log text, including the `kFullFramesRecord` block, which moves with the body unchanged.
- `UpdateTerminalReplayWriter`, `BuildExpectedReplayInventory`, `ComputeReplayGenerationDigest`, `PublishReplayManifest`, `DifferenceStreamWriter::Save`, and `game::WriteReplayMetadata`.

## Risk tier and invariants

Expected Tier 1 (trigger: a local, behavior-preserving move of one branch into a private member; no public signature, and the replay serialization surface is not altered).

- No format change: the same files are written in the same order with the same bytes; no replay version moves.
- The manifest stays the commit marker, published only after every generation and the metadata succeed (`Engine/Source/File/AGENTS.md` `## Replay Streams`).
- Server-only (`BT_SERVER`); no determinism/CRC, wire, threading, or trust change.

## Acceptance criteria

- The server builds through `/compile`.
- Through `/agent-harness`, a recording stopped by the changed server logs `Recording stopped` and passes the `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` `### Replay determinism acceptance` route.

## Coordination

- `Documents/Plans/Engine/MoveFullFramesTruncationToHarnessRigs.md` edits the `kFullFramesRecord` block (`Replay.cpp:842-882`) that this Plan moves into `StopRecording`. Whichever lands second rebases onto the other's edits.
- `Documents/Plans/Engine/SharedSha256Helper.md` edits the publish chain at `Replay.cpp:903`, which this Plan moves into `StopRecording`. Whichever lands second rebases onto the other's edits.

## Notes

- Line numbers are from `43450112`.
