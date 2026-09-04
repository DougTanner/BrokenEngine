<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T22:42:36.727Z","dependsOn":[]} -->
# Report a truthful `nextAction` at every `Complete-NextPlan.ps1` non-success terminal

## Context
`.agents/skills/next-plan/scripts/Complete-NextPlan.ps1:4` sets
`nextAction='finalize-changes'` as a constant inside the `$result` initializer,
and no later code path ever changes it. `Complete-Workflow` (`:8`) writes the
whole `$result` object at every exit, so the field is emitted unchanged on the
`no-claim` pass terminal (`:17`), on the claim-status failure terminal (`:16`),
on the prepare-failure terminal (`:23`), on the disposition-mismatch blocked
terminal (`:24`), and on both catch-block terminals (`:27`). Only the success
terminal (`:26`) is a case where `finalize-changes` is the true next action.

An agent that reads `nextAction` instead of decoding `status`/`code` is
therefore told to run `/finalize-changes` after a failed or blocked completion,
which would land a change whose Plan terminal state was never prepared.
`.agents/skills/next-plan/references/worker.md:69` already teaches agents to
consume this field ("and returns `nextAction: finalize-changes`").

The gap was found by the Change Workflow Step 2 `/plan-audit` (finding PA-F-005)
of `Documents/Plans/Engine/NextPlanScriptNextAction.md`, which adds `nextAction`
to the claim and defer scripts. That Plan's `## Out of scope` names
`Complete-NextPlan.ps1` ("already carrying `nextAction`"), so the fix is outside
its approved boundary; the user approved recording it here instead. This Plan is
independent and carries no `dependsOn` edge on that Plan, which is deleted when
it completes.

## Design
Recommended approach: stop treating `nextAction` as a constant and set it
alongside `status` and `code` at each terminal, using the four-value vocabulary
the sibling scripts adopt — `prepare`, `stop-report-to-user`,
`resume-with-flag`, and `retry-later` — plus `finalize-changes`, which stays the
success value. The author recommends no `nextActionDetail` field here, matching
the resolved design of the sibling change.

Recommended mapping, for the fix session to confirm against the script's own
terminals before implementing:

- `ok` (`:26`, exit `0`) — `finalize-changes`, unchanged.
- `no-claim` (`:17`, exit `0`) — `stop-report-to-user`: there is nothing to
  land, so finalization is not the next step.
- `completion.claim-status-failed` (`:16`), `completion.prepare-failed` (`:23`),
  `completion.disposition-mismatch` (`:24`), `completion.context-conflict` and
  `completion.failed` (`:27`) — `stop-report-to-user`, because each of these
  needs a human decision rather than an automatic retry.

The simplest mechanism is to add a `nextAction` parameter to `Complete-Workflow`
and assign `$result.nextAction` there, so no terminal can be added later without
choosing a value. The initializer keeps a value only as the unreachable
`internal.error` default.

`status` and `code` remain the authority; `nextAction` is derived from them and
must never contradict them.

## Critical files
- `.agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
- `.agents/skills/next-plan/references/worker.md` (step 9 prose that describes
  the returned `nextAction`)

## In scope
- The `$result` initializer and `Complete-Workflow` at
  `Complete-NextPlan.ps1:4` and `:8`, and every call site of `Complete-Workflow`
  in that script (`:16`, `:17`, `:23`, `:24`, `:26`, `:27`)
- The `next-plan` worker prose at `references/worker.md:69` that states what the
  script returns, if and only if the changed values make it inaccurate

## Out of scope
- The result schema name `broken-engine-next-plan-completion-result/v3`, every
  existing field, and each terminal's exit code, `status`, and `code`
- Adding a `nextActionDetail` field to this script
- `Invoke-NextPlanClaim.ps1` and `Defer-NextPlan.ps1`, which
  `Documents/Plans/Engine/NextPlanScriptNextAction.md` owns
- Any scheduler, claim-store, or WorktreeCli behavior change: the script reports
  differently, it never decides differently
- `Documents/Plans/AGENTS.md` scheduler rules

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one tool and the reference prose that reads
it, with no determinism, wire, serialization, threading, or trust-boundary
exposure); this author's classification, for main to confirm at Step 1.
Invariants to preserve: the schema name and every existing field; each
terminal's exit code, `status`, and `code`; the success terminal still returning
`finalize-changes`; the script still never mutating a claim on a failure path.

## Acceptance criteria
- Every terminal the script can reach returns a `nextAction` chosen for that
  terminal, and no terminal other than `ok` returns `finalize-changes`
- Each terminal's exit code, `status`, and `code` are byte-identical to today's
- `nextAction` uses only the four-value vocabulary plus `finalize-changes`
- `worker.md` step 9 still describes the field accurately

## Notes
Verification is by inspection of the script's terminals plus the existing
`next-plan` documentation; the mutation-capable script must not be run to
produce evidence, because running it changes a live claim.
