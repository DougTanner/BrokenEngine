---
name: next-plan
description: Validates and deterministically claims one Git-backed Documents/Plans Plan through WorktreeCli, resolves it against current code, and presents the resolved Plan and execution card for implementation approval. Use only when the latest user request explicitly asks to execute `/next-plan` or `$next-plan` and claim a Plan.
disable-model-invocation: true
argument-hint: "[Documents/Plans/... | partial pattern]"
allowed-tools: [Read, Write, Grep, Glob, Agent, Edit, PowerShell, AskUserQuestion]
---

# Next Plan

## Purpose

WorktreeCli alone validates metadata, selects, claims, prepares final state, and releases
claims; `Documents/Features` is never scheduler input. The whole Change Workflow in root
[AGENTS.md](../../../AGENTS.md) owns the cross-skill stage order, and
`/finalize-changes` the landing confirmation.

## When to use

Only when the latest user request explicitly asks to execute `/next-plan` or
`$next-plan` and claim a Plan. An audit, inspection, explanation, or quoted
mention of either name does not invoke this skill.

## Inputs

The `argument-hint` value selects the Plan:

- Bare invocation selects the newest eligible Plan by immutable `createdUtc`,
  then normalized UTF-8 path.
- A normalized `Documents/Plans/...` argument selects that Plan.
- Any other argument is a case-sensitive partial match against executable Plan
  paths relative to `Documents/Plans/`; exactly one match selects that Plan,
  and zero or multiple matches block.

## Steps

1. Resolve session context in a clean wrapper-created session worktree, as one
   shell call from its root:
`Import-Module ./.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1; Get-NextPlanContext`
   Done when it reports the authoritative primary, owner, and provisioned
   WorktreeCli; missing tooling first needs explicitly authorized primary
   maintenance through `/compile`.
2. Only when a decision needs the queue — a tier-constrained request, or the
   user asks to see the queue — see it before selecting:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Get-NextPlanList.ps1`
   It is read-only and reports a bounded point-in-time projection — widen with
   `-Top <n>` only when a decision needs more. A bare or plain named-Plan
   invocation skips this step, because step 3 selects and reports every stop
   itself. Done when this file's `## Inputs` resolves exactly
   one Plan or blocks.
3. Claim that selection as its own shell call, in the root AGENTS.md canonical
   invocation form — bare:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
   or with the requested normalized path or partial pattern quoted after
   `-Plan`, for example:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/example.md'`
   Done when the result reports `nextAction: prepare`.
4. Dispatch one preparation `implementer` to verify every Plan statement against
   current code, on the single task brief in
   [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md).

   That brief carries `Skill: none` — `/prepare-change` excludes a claimed
   executable Plan — and a `Return:` naming the shared handoff form plus the
   extension fields this file's `## Handoff` declares, so
   exactly one handoff contract binds the worker.

   The Plan is immutable, current code wins, and every delegation states that
   Plan and card statements are hypotheses, so every contradiction returns to
   main as a card correction rather than an edit.

   Main writes that brief from the claimed Plan's own citations — path plus
   line range — and leaves reading the target source to the dispatched
   `implementer`, reading source itself only for a decision it must make that
   the Plan and the returned handoff cannot settle.

   The brief bounds the card's verification evidence: each acceptance item
   comes back as a `path:line` citation plus a verdict, not verbatim source
   text, except an item whose purpose is proposed replacement text. A
   preparation handoff that returns a quantitative feasibility estimate as a
   Finding must name the check proving that the measured trial preserved the
   source document's rule or item inventory, or report the estimate as
   unvalidated. That is an expectation of the dispatch, not a gate the Done
   condition tests.

   When the preparation handoff will cite a scratch snapshot as the Plan
   review reviews' plan input rather than the claimed Plan path, the brief has
   the preparation `implementer` write the complete resolved Plan—its
   mechanism, corrections, the claimed Plan's `## In scope` and `## Out of
   scope` sections copied verbatim as top-level headings with their content
   intact, and the complete execution card—cite its path plus `## Execution
   card` selector under `Evidence` for the Plan review reviews, and, before
   returning that handoff, run
`pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Test-PlanCitations.ps1 <snapshot path>`
   and report that result's `headings.inScopePresent` and
   `headings.outOfScopePresent` values as one `Decisive checks` row.

   After that preparation handoff, and before the Plan review reviewers and the
   step 7 approval presentation, main runs `/plan-alternatives` when its trigger
   fires.

   Done when the execution card carries every field of the card template in
   this file's `### Execution card presentation/template`, the
   preparation handoff cites it as one file path plus `##` selector, and, when
   that handoff cites a scratch snapshot as the Plan review reviews' plan
   input rather than the claimed Plan path, its `Decisive checks` row for that
   snapshot reports `headings.inScopePresent` and
   `headings.outOfScopePresent` both `true`.
5. Run the Plan review step in root
   [AGENTS.md](../../../AGENTS.md), after preparation and alternatives and
   before the final claim refresh. Tier 3 retains the additional route in
   [tier3-workflow.md](references/tier3-workflow.md). Done when every required review has
   completed and accepted findings are resolved, or a missing mandatory
   reviewer is reported as a blocker.
6. Invoke step 3's claim script idempotently immediately before the final
   preparation handoff. When that result carries a `sync` object, the tree
   moved under the preparation evidence: diff `sync.from..sync.to` against the
   paths the preparation handoff cited. When they intersect, return to step 4,
   rerun the affected Plan review checks in step 5, and repeat this final refresh
   before approval; reuse review evidence whose inputs did not change. Done when
   it reports the held claim and the preparation and required Plan review
   evidence match the current tree.
7. Present for approval per this file's `### Implementation approval`. Done
   when the user's decision arrives.
8. Implement the approved change. Done when its own acceptance checks pass.
9. Run the checkpoint exactly once, however the run ends: after step 8's
   acceptance checks, before step 10 when that step applies, and before
   `/finalize-changes` prepares the
   landing commit, so a Plan it produces is an ordinary new worktree file riding
   the same squash with no landing-commit join. Every terminal path of the run
   reaches this step before the run ends: a `none-available` or other claim
   stop, a deferral, a blocker, or a refused approval comes straight here from
   wherever it stopped, while an implemented run keeps the position above.

   Main never performs either review itself; it dispatches them per
   [run-checkpoint.md](references/run-checkpoint.md). Done when both follow-up lines are
   recorded per run-checkpoint.md.
10. Only after implementation is accepted and verified, or after the user
   explicitly authorizes rejection, exit the held claim before landing-commit
   creation: an `implementer` runs
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
   with no arguments for completion, appending `-Reject` only after explicit
   user-authorized rejection.

   Success removes only direct-child dependency edges, deletes the selected Plan
   in the worktree, reports the changed paths the landing commit must contain,
   and returns `nextAction: finalize-changes`. Done when that result is in hand;
   the claim stays held until landing succeeds, and `/finalize-changes` deletes
   it after primary advances.
11. End the run per this file's `### IMPORTANT: Session-complete marker`: print
   `SESSION COMPLETE` as the last line only
   when the `/finalize-changes` handoff carried it. Done when the final
   message's last line matches that rule for the run's outcome.

### Post-checkpoint outcomes

| Run outcome | Claim disposition | Next action |
| --- | --- | --- |
| implementation accepted and verified | held | run `Complete-NextPlan.ps1`, then the landing gate |
| user explicitly authorizes rejection | held | run `Complete-NextPlan.ps1 -Reject`, then the landing gate |
| claim wrapper returns `claim.plan-mismatch` | held, identified only by `conflict.heldPlan` | run the checkpoint once, retain the held claim and all work, report requested and held Plans, and stop; never complete, reject, or defer implicitly |
| approval refused, or a blocker whose authoritative result explicitly reports a held claim, with no separate defer instruction | held | run the checkpoint once, retain the claim and all work, report and stop; never complete, reject, or defer implicitly, and any checkpoint work created stays with the retained session |
| blocker before an authoritative claim disposition is available | unknown | follow the supplied `nextAction`, run the checkpoint once, report the error and unknown claim state, and stop, retaining all work with the session; `claim: null` alone never proves claim absence, so add no recovery probe and do not query claim status or mutate a claim |
| user separately and explicitly directs deferral, or the run is already deferred | released/absent only through `Defer-NextPlan.ps1`'s documented result | run the checkpoint once; if it creates tracked follow-up content, use a followup-only landing gate, which runs no Plan completion or rejection and releases no other held claim, otherwise report and stop |
| authoritative result explicitly reports that no claim is held, including `none-available` | absent | run the checkpoint once; if it creates tracked follow-up content, use a followup-only landing gate as the previous row defines it, otherwise report and stop |

## Handoff

The preparation handoff extends the shared form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
with the declared fields below. Main's brief bounds the returned handoff to
every contradiction and unresolved decision, the other verified Plan statements
whose result requires a card or implementation change, and one count of the
unaffected statements; it never asks for a per-statement enumeration of
unaffected results.

- `Claim` — the Plan path, or none; with the resolved state when claimed.
- `Classification` — `Tier 1`, `Tier 2`, or `Tier 3`, and the trigger.
- `Execution card` — the file path plus its `##` selector.

### Execution card presentation/template

`Execution card` names the file and `##` selector containing the following
required content. Main reads that content and presents it with the resolved
Plan; the card itself is not repeated inline in the handoff.

```text
Execution card:
### What does this plan do?
<2-4 plain sentences>
### Why this is good for the codebase
<2-4 plain sentences>
- Goal: <result>
- Out of scope: <boundary>
- Tier trigger: <trigger or none>
- Interfaces and invariants: <contracts>
- Acceptance checks: <check and expected observation>
- Roles: <required and conditional assignments>
```

### Implementation approval

Preparation and claim do not require approval. Present the complete resolved
Plan and execution card before implementation: scope, invariants, role
assignments, acceptance criteria, and unresolved decisions. Deliver that
presentation per the User Interaction rules in root
[AGENTS.md](../../../AGENTS.md) — on Codex as exactly one complete
`<proposed_plan>` block, then ending the turn without an approval question; on
Claude Code and every other host as rendered message text whose approval
question is the last thing before the `Follow-up Plans created:` footer, after
which the user's next message is the decision. Any revision is a new complete
replacement presentation.

When preparation shows the problem the Plan describes is gone, ask the user
whether to retain the Plan or to explicitly authorize obsolete final cleanup.

### IMPORTANT: Session-complete marker

The user closes the session tab on this line alone, so it must be
unambiguous. When, and only when, the `/finalize-changes` handoff carries its
`SESSION COMPLETE` line under the conditions its `## Handoff` states — the
Plan was completed or rejected and its file removed, landing onto primary
succeeded, the claim was released, the worktree is clean, and every objective
stage is complete or explicitly deferred to a named unclaimed Plan — main's
final message of that turn ends with the bare line `SESSION COMPLETE` as its
very last line, after the `Follow-up Plans created:` footer, and asks the user
nothing. Every other ending — retained claim, blocker, refused approval,
deferral, unknown claim state, or work still to land — never prints that line.

## Rules

- The preparation worker must never run the mutation-capable claim script.
- Every result is acted on per its `nextAction`, and for a `-Plan`-targeted
  invocation the manager never selects or claims a different candidate in that
  run.
- Never create or adopt a worktree, or inspect machine-local claims directly.
- Which reviewer runs at which tier is the Plan review step of root
  [AGENTS.md](../../../AGENTS.md); Tier 3 additionally follows
  [tier3-workflow.md](references/tier3-workflow.md). Missing a mandatory reviewer blocks.
- An affirmative response approves only the latest unchanged presentation. A
  meaningful Plan, card, scope, invariant, acceptance, or decision change
  requires a new complete presentation.
- Deferral uses
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Defer-NextPlan.ps1`
  and only an ordinary live claim. After final preparation has run, deferral
  requires an explicit user instruction given in the current session, recorded
  in the handoff; nothing else unlocks it. Deferral never touches the worktree,
  so uncommitted implementation work stays exactly as it is.
- Resuming retained work needs an explicit user resume instruction given in the
  current session, recorded in the handoff, and then the targeted claim with
  `-ResumeRetained` appended:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/example.md' -ResumeRetained`
  That switch is valid only with `-Plan` and changes no file. A retained-work
  resume is the one exception to step 1's clean wrapper-created worktree.
- On Claude, the checkpoint review covers friction, oversized-result telemetry,
  and isolation evidence after the previous checkpoint's dispatch in the same
  transcript, or from transcript start when none exists, through immediately
  before its own dispatch, excluding only the previous checkpoint's correlated
  completed handoff; `/next-plan-review` covers the rest after landing. Codex
  runs no live checkpoint lens and leaves all three concerns to
  `/next-plan-review`.
- [claim-results.md](references/claim-results.md) owns how each claim, listing, and
  claim-exit result is read, including the `sync` and `nextAction` fields, the
  listing's snapshot limits and tier-constrained reading procedure, and the
  completion and retained-path result shapes. Do not reconstruct any script's
  transitions.
- [follow-up-provenance.md](references/follow-up-provenance.md) owns where the provenance
  block's values come from.

## References

- [references/claim-results.md](references/claim-results.md) — how each claim,
  listing, and claim-exit result is read.
- [references/tier3-workflow.md](references/tier3-workflow.md) — the additional
  Tier-3 preparation route.
- [references/run-checkpoint.md](references/run-checkpoint.md) — the end-of-run
  checkpoint mechanics.
- [references/follow-up-provenance.md](references/follow-up-provenance.md) —
  which session ID applies and the provenance block.
