<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:49.354Z","dependsOn":[]} -->
# Assign the audio harness rig snapshot once in its command handler

## Context

Found by a deep analysis of `Engine/Source/Agent` (in-function phase),
verified by code reading against commit `4fa6d2d7`; nothing was built. Line
numbers are from the commit that added this Plan (locate by symbol if they
moved) and are in
`Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp` unless a path
is given.

`CommandAudioStreamingHarnessRig` writes
`rResult = BuildAudioStreamingHarnessRigSnapshot(); return;` twelve times
(`:275-276`, `285-286`, `292-293`, `299-300`, `305-306`, `311-312`,
`317-318`, `324-325`, `358-359`, `364-365`, `370-371`, `376-377`). No action
builds a different result. The only other exits are the two `DeferResponse`
returns (`:327-335`, `:340-353`), the three throws (`:273`, `:283`, `:379`),
and the validation throws. Every new action has to repeat the tail.

Unmet criterion: the snapshot result is assigned once for every non-deferred
action.

## Design

Turn the action tests of `CommandAudioStreamingHarnessRig` (`:268-379`) into
an `if`/`else if` chain that falls through to one
`rResult = BuildAudioStreamingHarnessRigSnapshot();` as the last statement of
the Debug body. No helper is added.

The chain cannot be flat. `RequireAudioStreamingHarnessRigFields(rParameters,
{})` at `:296` must run after the three field-taking actions and before any
no-field action's side effect (`Clear`, `Suspend`, `StallLoaders`, the flag
writes). So the shape is:

- Outer chain: `play`, `hold_read`, `load_chunk`, each keeping its own
  `RequireAudioStreamingHarnessRigFields` call and its throw, then a final
  `else`.
- That `else` holds the `:296` check, then an inner chain over the no-field
  actions in their current order: `inspect`, `clear`, `suspend`, `resume`,
  `clear_history`, `release_read`, `stall_loaders`, `release_loaders`,
  `stop_publication`, `resume_publication`, then a final `else` that throws
  the schema string as `:379` does now.
- `inspect` is a branch with an empty body: it has no side effect and only
  keeps the action out of the final `else`.
- `clear_history` calls `DeferResponse` and returns only when `BeginHistory()`
  returns false; when it returns true the branch ends and falls through to
  the one assignment.
- `release_read` keeps its `DeferResponse` and its `return`.

`:296` and `:379` throw the same schema string, so which of them rejects an
unknown action is not observable.

Risk tier: Tier 1 (the author's classification). Trigger: local
behavior-preserving work inside one function with no signature change. The
function is `BT_CLIENT`, and its body is Debug only (`:254-256`); no
determinism, wire, serialization, or threading exposure.

## Critical files

- `Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`

## In scope

- The action tests of `CommandAudioStreamingHarnessRig` (`:268-379`) and its
  twelve snapshot tails.

## Out of scope

- Error text, action names, required fields, and the order of side effects.
- The two deferred poll lambdas, apart from staying inside their branches.
- The sentinel literal at `:347`.
- Every other dispatch chain under `Engine/Source/Agent`.

## Acceptance criteria

1. Code read: `CommandAudioStreamingHarnessRig` assigns
   `BuildAudioStreamingHarnessRigSnapshot()` to `rResult` once; the `:296`
   check still runs after the three field-taking actions and before every
   no-field action's side effect.
2. `/compile` builds the BrokenEngineSandbox client in Debug and Release.
3. A Debug client run through `/agent-harness` sends
   `audio_streaming_harness_rig` once per action, once with an unknown
   action, and once with `inspect` plus an extra field; each returns the same
   result shape or error text as before the change. The supplied evidence
   records no harness setup, because the finding was proven by code reading
   only.

## Notes

The value is small: about twenty lines removed for one added nesting level
and one empty branch. The shape also departs from the early-return dispatch
form the directory uses for command dispatch
(`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1255-1275`), which is
why `## Design` states the shape instead of leaving it to the executing
session.

## Coordination

- `Documents/Plans/Engine/NameAudioHarnessRigNoPoolIndex.md` names the
  sentinel at `:347`, inside the `release_read` branch this Plan moves into
  the inner chain. Neither Plan needs the other first; whichever lands
  second applies its edit to that function as the other left it.
