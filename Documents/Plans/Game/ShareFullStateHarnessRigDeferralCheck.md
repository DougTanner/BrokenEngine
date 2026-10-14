<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:44.399Z","dependsOn":[]} -->
# Share the full-state harness rig deferral check between its two exercise actions

## Context

`CommandClientFullStateHarnessRig` (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientFullStateHarnessRig.cpp:221-422`) runs `exercise_matching_tick` through the sibling helper `ExerciseMatchingTick` (153-219), but runs `exercise_gap` inline (326-412), and that inline body copies the helper nearly line for line:

- cell lookup and pending-full-state checks: 329-345 against 155-171;
- the deferring `Reconcile` run and its preserved-pending check: 347-354 against 173-180;
- the clock force to the pending full-state tick and time: 372-377 against 190-196;
- six result fields `pendingTick`, `deferTargetTick`, `beforeDefer`, `afterDefer`, `deferPendingPreserved`, `deferDesync`: 395-400 against 207-212.

Only the update-gap creation (356-370) and the adoption checks and their fields (380-393, 401-412) are gap-specific; the contiguous-update check (182-188), `rbClockForced`, and the injection fields are matching-specific.

Origin: a user-directed read-only review of the top 20 functions by complexity mass at `43450112`. The user's rule: a function is never split because it is big; code that belongs together stays together, and only a piece that does not belong moves. The copy is the piece; the dispatcher's validation, `clear`, `arm_stall`, armed checks, and `inspect` were judged to belong and stay as is.

## Design

Recommended helpers, both file-static beside `ExerciseMatchingTick`:

- `static engine::Cell& ExerciseDeferral(engine::GridCoord coordinate, nlohmann::json& rResult)`: the cell lookup, both pending checks, the ahead-of-client-tick check, the deferring `Reconcile` run with `beforeDefer`/`afterDefer`, and the preserved-pending throw, with the current messages; it writes the six common fields and returns the cell. The caller reads `iPendingTick` back as `rCell.pendingFullState->iTick`, which the helper has just proven equals the original pending tick. Rationale: returning only the cell avoids an out-parameter or a new result struct.
- `static void ForceClockToPendingFullState(const engine::Cell& rCell)`: the `miTickCounter`, `mfCurrentTime`, and `mTickRemainderNanoseconds` writes with their `ASSERT`. `ExerciseMatchingTick` keeps `rbClockForced = true;` immediately before calling it.

`ExerciseMatchingTick` and a new `static void ExerciseGap(engine::GridCoord coordinate, nlohmann::json& rResult)` each call `ExerciseDeferral`, then their own specific step, then `ForceClockToPendingFullState`, then their own checks and fields. The `exercise_gap` branch becomes a `try { ExerciseGap(sHarnessRig.coordinate, rResult); } catch (...) { ClearHarnessRig(); throw; }` followed by its current `ClearHarnessRig()` and `cleared`, parallel to the `exercise_matching_tick` branch.

The six common fields are now written before later checks that can still throw. That is not observable: the transport moves the handler's local `result` into the response only on success (`Engine/Source/Agent/AgentCommandServer.cpp:387-394`). `nlohmann::json` objects are key-ordered, so the response text is unchanged.

Check during implementation, not a decided fix: the two actions clear the rig on failure differently. `exercise_matching_tick` clears only after the clock was forced (308-314), while `exercise_gap` clears on any failure (414-418), including the precondition throws at 332, 336, and 344, before any rig or cell state changed. Evidence that the matching path's policy is deliberate: `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md` says `exercise_matching_tick` fails while updates are missing and is retried after more arrive, which needs the rig to stay armed. Nothing found documents why `exercise_gap` clears on a precondition failure. This Plan keeps both policies exactly as they are; when the check finds the gap policy unintended, report it as a residual for the user rather than changing it here, because that is a behavior change.

Risk tier: Tier 1 (`.agents/references/risk-tiers.md`). Trigger: a local behavior-preserving de-duplication into file-static helpers in one translation unit, with no public signature or invariant exposure. The agent command channel is an exempt developer tool (`Engine/Source/Agent/AGENTS.md` `## Architecture`). The forced clock and the `Reconcile` runs keep their order and values, so client reconciliation behavior is unchanged.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientFullStateHarnessRig.cpp`

## In scope

- New file-static `ExerciseDeferral`, `ForceClockToPendingFullState`, and `ExerciseGap` in `ClientFullStateHarnessRig.cpp`.
- `ExerciseMatchingTick` (153-219), rewritten to call the two shared helpers.
- The `exercise_gap` branch of `CommandClientFullStateHarnessRig` (326-418), replaced by the `ExerciseGap` call and its existing catch and clear.

## Out of scope

- Either action's clear-on-failure policy (see the check in Design).
- `BuildCoordinateState`, `BuildState`, `ClearHarnessRig`, `ResetHarnessRig`, `IsHarnessRigStalled`, `DetachClientFullStateHarnessRig`.
- The validation, `clear`, `arm_stall`, armed checks, and `inspect` paths of `CommandClientFullStateHarnessRig`.
- Any result field name or value, any throw message, and `commands-client.md`.

## Acceptance criteria

- Client builds (`/compile`).
- `/agent-harness`: the documented `client_full_state_harness_rig` `exercise_gap` sequence in `commands-client.md` passes every requirement it lists, and `arm_stall` followed by `exercise_matching_tick` returns the documented field set with `pendingCleared` true.

## Notes

- Report the outcome of the clear-on-failure check in the implementation handoff either way.
