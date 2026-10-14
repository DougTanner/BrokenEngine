<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:31.569Z","dependsOn":[]} -->
# Move the malformed-fullframes harness hook out of replay persistence into ReplayHarnessRigs

## Context

When recording stops, the writer-save loop in `Replay::SyncReplayTick` (`Engine/Source/File/Replay.cpp:842-882` at `43450112`) carries a harness test hook inline on the persistence path. When `ReplayHarnessRigs::ConsumePersistenceFailure(..., kFullFramesRecord, coord, activationTick)` fires after a successful `Save`, it reads the saved `.fullframes` sibling back, rewrites it one byte shorter with `WriteFileAtomically`, and logs `Injected malformed replay fullframes ...`. If any step fails it logs an error, calls `CleanupFiles` on the writer's sibling set, and clears `bWriterSaved`. The harness command `replay_inject_persistence_failure {"stage":"fullframes_record"}` arms it (`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` `#### Command-only malformed fullframes route`).

`Engine/Source/File/AGENTS.md` `## Replay Streams` places server replay harness rig controls in the engine Agent's Replay-bound slot, `ReplayHarnessRigs` (`Engine/Source/Agent/Commands/ReplayHarnessRigs.h`/`.cpp`), and keeps ordinary recording and persistence in File. This hook is a rig control that sits in File. The other failure points only gate a persistence step through `ConsumePersistenceFailure`; this is the only one that performs file I/O of its own inside `Replay`.

Origin: a read-only review of the top 20 functions by complexity mass at `43450112`. The user's governing rule, as relayed by the dispatching agent: a function is never split only because it is big; only a piece that is a separate job fused into its function moves. This hook was judged such a piece.

## Design

Author's recommendation, with rationale:

1. Add `[[nodiscard]] bool InjectMalformedFullFrames(const Replay& rReplay, GridCoord coordinate, int64_t iActivationTick, const FileFlags_t& rFileFlags, const std::filesystem::path& rCoordinateReplayPath);` to `ReplayHarnessRigs.h`, defined in `ReplayHarnessRigs.cpp`. It calls `ConsumePersistenceFailure(rReplay, PersistenceFailurePoint::kFullFramesRecord, coordinate, iActivationTick)`. When that returns `false` it returns `true`. Otherwise it runs the moved read-back, truncate, and rewrite body (`Replay.cpp:844-873`) and both logs (`:874-881`), and it returns whether the truncation succeeded.
2. The cleanup stays in `Replay`, because deleting a torn sibling set is persistence behavior that `## Replay Streams` keeps in File. The rig therefore needs no writer: `if (bWriterSaved && !ReplayHarnessRigs::InjectMalformedFullFrames(*this, rCoordinate, rWriterState.iActivationTick, fileFlags, coordinateReplayPath)) { rWriterState.pWriter->CleanupFiles(fileFlags, coordinateReplayPath); bWriterSaved = false; }`.
3. The error log moves into the rig with the rest of the hook, so its text and its order relative to the cleanup do not change.

## Critical files

- `Engine/Source/File/Replay.cpp`
- `Engine/Source/Agent/Commands/ReplayHarnessRigs.h`
- `Engine/Source/Agent/Commands/ReplayHarnessRigs.cpp`

## In scope

- New `ReplayHarnessRigs::InjectMalformedFullFrames`: its declaration and definition.
- The `kFullFramesRecord` block in the record-stop writer-save loop (`Replay.cpp:842-882` at `43450112`; it is inside `Replay::StopRecording` instead if `Documents/Plans/Engine/ExtractReplayStopRecording.md` has landed), replaced by the call and the cleanup.

## Out of scope

- `ConsumePersistenceFailure`, `ArmPersistenceFailure`, the `PersistenceFailurePoint` enum, and the other failure points.
- `ServerSimulationHarnessRigs.cpp`, the `replay_inject_persistence_failure` command, and its documentation.
- `DifferenceStreamWriter::Save` and `CleanupFiles`, and every other line of the record-stop branch.

## Risk tier and invariants

Expected Tier 3 (trigger: a change spanning independently owned subsystems, Engine Agent (`ReplayHarnessRigs`) and Engine File (`Replay`), that adds a `ReplayHarnessRigs` entry point).

- Behavior-preserving: when the hook is not armed, persistence is byte-identical. When it is armed, the same file is rewritten to the same bytes, the same lines are logged, and the same failure deletes the same sibling set.
- No format change; no replay version moves. Server-only (`BT_SERVER`); no determinism/CRC, wire, or threading change.

## Acceptance criteria

- `Replay.cpp` no longer opens, reads, or rewrites `.fullframes`, and contains no `Injected malformed replay fullframes` text.
- The server builds through `/compile`.
- Through `/agent-harness` on Debug, the `#### Command-only malformed fullframes route` in `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` passes unchanged: the before/after sizes differ by exactly one, the manifest validates, the `Full frames file read failed, discarding` warning appears, and playback loops with no CRC or desync errors.

## Coordination

- `Documents/Plans/Engine/ExtractReplayStopRecording.md` moves this block, unchanged, into `Replay::StopRecording`. Whichever lands second rebases onto the other's edits.

## Notes

- Line numbers are from `43450112`.
