<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:51:59.689Z","dependsOn":[]} -->
# Bound deferred hover scripts to the agent response lifetime

## Context

The retained survivor `CAI/shard-0010/002` shows that the hover command accepts
a duration that outlives the only response channel's deferred timeout.
`CommandHover` copies an external integer through a zero-only clamp at
`Engine/Source/Agent/AgentCommandsClientGeneric.cpp:899-909`.
`AgentInput::AdvanceFrame` finishes a hover only after
`mScript.iHoldFrames` at `Engine/Source/Agent/AgentInput.cpp:229-245`, while
the timeout branch in `AgentCommandServer::Drain` clears only `mDeferredPoll`
and publishes an error at `Engine/Source/Agent/AgentCommandServer.cpp:289-297`.
It does not finish the active `AgentInput` script.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0010.md:52`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:524`. The frozen/live
source rows match baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`, and no
source was changed by this routing session. The neighboring key command already
uses `kiMaxKeyHoldFrames` at `AgentCommandsClientGeneric.cpp:33-53`, so this is
an unresolved command-boundary omission rather than a deliberate unlimited
duration.

## Design

The author's recommendation is to validate `hover.holdFrames` against the
deferred drain lifetime, with the same stabilization slack used by the existing
bounded key command, before `BeginScriptAndDefer` is reached. Reject a duration
that cannot finish and publish before the response timeout; retain zero and
ordinary short durations. This keeps the one-script state machine unchanged
and avoids making the shared timeout path reach into private `AgentInput` state.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:33-53,899-909` — input
  duration parsing and hover command admission.
- `Engine/Source/Agent/AgentInput.cpp:229-245` and
  `Engine/Source/Agent/AgentInput.h:54-62,113-117` — script lifetime state.
- `Engine/Source/Agent/AgentCommandServer.cpp:289-317` — deferred timeout
  contract (reference for the bound, not a new timeout policy).
- `Engine/Source/Agent/AGENTS.md` — one-script and bounded-deferred-liveness
  rules.

## In scope

- The external `hover.holdFrames` validation and its relation to the existing
  deferred response bound.
- Error publication for an out-of-range duration before script state changes.
- Retaining normal hover stabilization, completion, and UI pin behavior for
  accepted durations.

## Out of scope

- The command wire format, deferred timeout constant, or other input commands.
- A general script cancellation API, unless the chosen bound cannot satisfy the
  existing timeout contract.
- ImGui target resolution and unrelated synthetic-input publication.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: an opaque
agent command controls a main-thread script and a background deferred response
channel; accepted input must remain bounded by transport liveness.

Tier rationale: the fix reuses the bound the neighboring key command already
applies, as one admission check in `CommandHover` before any script state is
installed; nothing in the threading structure, timeout policy, or command wire
format changes.

Preserve these invariants:

- Every accepted hover completes before the deferred response deadline or is
  rejected before any script state is installed.
- A timed-out response cannot leave `mbScriptActive`, the synthetic state, or
  the sole command channel wedged.
- Existing zero/short-hover semantics and one-script exclusion remain intact.

## Acceptance criteria

- A hover with a duration above the documented bound is rejected immediately
  and leaves the input engine idle.
- The maximum accepted duration completes with an ordinary deferred success,
  while a short hover still stabilizes and publishes its UI snapshot.
- After any accepted hover, the next key, mouse, or click command is accepted;
  no timeout path leaves the script active.
- Client `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0010/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:524`. No source fix or build
was performed during routing.
