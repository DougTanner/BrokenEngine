<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:37.667Z","dependsOn":[]} -->
# Extract the audio streaming saturate step from the harness rig dispatcher

## Context

`CommandAudioStreamingHarnessRig` (`Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp:391-801`) validates `action` and dispatches ten actions. Nine are short. The tenth, `saturate` (594-798), defers a poll lambda (604-797) that is a 190-line five-phase state machine over `AudioStreamingHarnessRigSaturationPhase` (`kSetup`, `kWaitForPrimaryStream`, `kWaitForMandatoryStream`, `kWaitForRetry`, `kWaitForSettled`; 331-338). The lambda reads only its captured `pState`, the global harness rig, and file-scope helpers; every value it carries across polls already lives in `AudioStreamingHarnessRigSaturationState` (340-353). It shares no local with the dispatcher or with the other actions, so it is a separate job fused into the dispatcher, and it carries about 40 of the dispatcher's cyclomatic complexity.

Origin: a user-directed read-only review of the top 20 functions by complexity mass at `43450112`. The user's rule: a function is never split because it is big; code that belongs together stays together, and only a piece that does not belong moves. The rest of `CommandAudioStreamingHarnessRig` — the action validation and the other nine actions — was judged to belong and stays as is.

## Design

Recommended: add `static std::optional<nlohmann::json> StepAudioStreamingHarnessRigSaturation(AudioStreamingHarnessRigSaturationState& rState)` in the existing `BT_DEBUG` helper block, after `IsAudioStreamingHarnessRigSaturated` and before that block's `#endif` (389). Move the lambda body (606-796) into it unchanged apart from `pState->` becoming `rState.`. The `saturate` call site becomes `engine::gpAgentCommandServer->DeferResponse([pState] { return StepAudioStreamingHarnessRigSaturation(*pState); });`.

Rationale: the state struct already owns every cross-poll value, so the function needs no new parameter or type. The lambda keeps its `std::shared_ptr` capture, so the state destructor's release calls (342-347) still run when the deferred poll is destroyed.

Unchanged: the pre-defer code at 594-603 (first-action gate, `RequestHold`, `kSaturationActive`) stays in the dispatcher; phase order, every check, every throw message, both `ScopedSuppressAllocationTracking` scopes, and the returned JSON stay as they are.

Risk tier: Tier 1 (`.agents/references/risk-tiers.md`). Trigger: a local behavior-preserving move of one block into a file-static function in one translation unit, with no public signature or invariant exposure. The agent command channel is an exempt developer tool (`Engine/Source/Agent/AGENTS.md` `## Architecture`). No CRC, wire, serialization, replay, or threading surface is touched; the poll still runs on the main thread from `Drain()`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`

## In scope

- New file-static `StepAudioStreamingHarnessRigSaturation` in the `BT_DEBUG` helper block of `AgentCommandsAudioStreaming.cpp`.
- The `saturate` `DeferResponse` lambda in `CommandAudioStreamingHarnessRig` (604-797), replaced by the forwarding lambda.

## Out of scope

- The other nine actions, the action validation (402-428), and the saturate pre-defer code (594-603).
- `AudioStreamingHarnessRigSaturationState`, `AudioStreamingHarnessRigSaturationPhase`, `IsAudioStreamingHarnessRigSaturated`, and every other helper in the file.
- Any change to phase logic, thresholds, messages, or result JSON, and any further split of the step function, such as one function per phase.
- The engine `AudioStreamingHarnessRig` and the harness documents.

## Acceptance criteria

- Client Debug builds (`/compile`).
- `/agent-harness`: in a fresh Debug agent client, `audio_streaming_harness_rig` `{"action":"saturate"}` as the first harness rig action returns an object holding `saturated` and `settled` snapshots, as before.

## Notes

- The dispatcher body is client `BT_DEBUG` only (397-800); the server build does not compile it.

## Coordination

- `Documents/Plans/Engine/AudioStreamingHarnessRigSignedCounters.md` renames `uiRetryCount` to `iRetryCount`, which the moved body reads in `kWaitForRetry` (`AgentCommandsAudioStreaming.cpp:718`). Whichever lands second rebases onto the other's edits.
