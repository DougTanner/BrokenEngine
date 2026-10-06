---
name: next-plan
description: Validates and deterministically claims one Git-backed Documents/Plans Plan through WorktreeCli, resolves it against current code, and presents the execution card's summary, alternatives, and unresolved decisions for implementation approval. Use only when the latest user request explicitly asks to execute `/next-plan` or `$next-plan` and claim a Plan.
disable-model-invocation: true
argument-hint: "[Documents/Plans/... | partial pattern]"
allowed-tools: [Read, Write, Grep, Glob, Agent, Edit, PowerShell, AskUserQuestion]
---

# Next Plan

## Purpose

WorktreeCli alone validates metadata, selects, claims, prepares final state, and releases
claims; `Documents/Features` is never scheduler input. The whole Change Workflow in
[change-workflow.md](../../references/change-workflow.md) owns the cross-skill stage order, and
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
   [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
   `## Task brief`.

   That brief carries `Skill: none` — `/prepare-change` excludes a claimed
   executable Plan — and a `Return:` naming the shared handoff form plus the
   extension fields this file's `## Handoff` declares, so
   exactly one handoff contract binds the worker. It also carries the Change
   Workflow step and role assignments the claimed Plan triggers, as main states
   them from the workflow.

   The Plan is immutable, current code wins, and every delegation states that
   Plan and card statements are hypotheses, so every contradiction returns to
   main as a card correction rather than an edit.

   Main writes that brief from the claimed Plan's own citations — path plus
   line range — citing the claimed Plan as its path plus the section line
   ranges a `^## ` heading Grep of it returns, and reading only the Plan
   sections it must state from, such as the risk tier that sets the role
   assignments, just as it reads only the cited `## Task brief` section above,
   and leaves reading both the target source and the handoff and reporting
   references the brief cites to the dispatched `implementer`, naming those
   references as paths only, and reading source itself only for a decision it
   must make that the Plan and the returned handoff cannot settle.

   The brief bounds the card's verification evidence: no verbatim source
   text, except an acceptance item whose purpose is proposed replacement
   text. A
   preparation handoff that returns a quantitative feasibility estimate as a
   Finding must name the check proving that the measured trial preserved the
   source document's rule or item inventory, or report the estimate as
   unvalidated. That is an expectation of the dispatch, not a gate the Done
   condition tests.

   The brief has the preparation `implementer` write the complete resolved
   Plan—its mechanism, corrections, the claimed Plan's `## In scope` and
   `## Out of scope` sections copied verbatim as top-level headings with their
   content intact, and the complete execution card—into one gitignored `Temp/`
   file addressed by its path from the worktree root, each under its own `##`
   heading, with a `## Unresolved decisions` section (`none` when every
   decision is resolved), cite that path under `Evidence` with one `##`
   selector per section for the Plan review reviews and the approval
   presentation, and,
   before returning that handoff, run
`pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Test-PlanCitations.ps1 <snapshot path>`
   and report that result's `headings.inScopePresent` and
   `headings.outOfScopePresent` values as one `Decisive checks` row. Of that
   result, only those two booleans gate this step's Done condition; every other
   record it carries, including a `citations.items` entry with `pathExists` or
   `lineExists` false, is an advisory lead, never a defect to clear and rerun.

   The brief also has the preparation `implementer` check the planned change
   against
   [`../compile/references/runtime-data-mode.md`](../compile/references/runtime-data-mode.md)
   `## Mode selection` and, when it hits a Local trigger, list the Local
   generation authorization request `/compile` `## Inputs` requires at plan
   approval under the snapshot's `## Unresolved decisions`, with the
   consequence of declining — no BrokenEngineSandbox build can generate the
   Local data the change needs — and a recommendation to grant. The
   authorization rests only on that item as presented under
   `### Implementation approval`, never on an approval that did not carry it.

   After that preparation handoff, and before the Plan review reviewers, main
   runs `/plan-alternatives` when its trigger fires and presents per that
   skill's `### User presentation`.

   Done when the execution card carries every field of the card template in
   this file's `### Execution card presentation/template`, the preparation
   handoff cites that snapshot as one file path plus the `##` selectors this
   step requires, and its `Decisive checks` row for that `Temp/` snapshot
   reports `headings.inScopePresent` and `headings.outOfScopePresent` both
   `true`.
5. Run the Plan review step in
   [change-workflow.md](../../references/change-workflow.md), after preparation and alternatives and
   before the final claim refresh. Tier 3 retains the additional route in
   [tier3-workflow.md](references/tier3-workflow.md). Done when every required review has
   completed and accepted findings are resolved, or a missing mandatory
   reviewer is reported as a blocker.
6. Unless step 10 already ran (the changes-requested row in
   `### Post-checkpoint outcomes`), invoke step 3's claim script idempotently
   immediately before the final preparation handoff. When that result carries
   a `sync` object, the tree moved under the preparation evidence, and main
   never lists or reads that
   range itself: it dispatches one `researcher` with `sync.from..sync.to`, the
   preparation snapshot's path and `##` selectors, and the preparation
   handoff's one-line rows that cite repository paths outside that snapshot.
   That worker lists the changed names with
   `git diff --name-only --no-renames <sync.from> <sync.to>`, keeps those the
   snapshot or those rows cite, including skill-relative (resolving against
   the directory of the skill whose `SKILL.md` or name the surrounding text
   cites), directory, and skill-name (`/<skill>`, resolving to that skill's
   `.agents/skills/<name>/` files) citations, and returns under
   `Decisive checks`, without quoting the diff or the uncited names, one row
   with the count of changed cited paths and one row per changed cited path
   stating whether its change touches a statement the preparation handoff or
   execution card relies on.
   When any row reports a touch, return to step 4, rerun the affected Plan
   review checks in step 5, and repeat this final refresh before approval;
   reuse review evidence whose inputs did not change. Done when the claim
   result reports the held claim and either the claim result carries no `sync`
   object, the count row reports zero, or every per-path row reports no touch.
7. Present for approval per this file's `### Implementation approval`. Done
   when the user's decision arrives.
8. Implement the approved change. Main dispatches one `/implement-plan`
   `implementer` per `Slices` row of the latest handoff that returned that
   field, each brief citing as the approved plan the step-4 preparation
   snapshot (the `Temp/` path whose `## In scope` and `## Out of scope` the
   preparation handoff cites, or the difference preparation's snapshot on the
   changes-requested route) and copying that row as the assigned items and
   allowed file scope; main never reads the snapshot's `## In scope` itself.
   Main assigns each change the user approved after the plan to exactly one
   row, adding one slice when none fits, and passes `none` for those changes
   to every other row. `Slices: none`
   dispatches no implementer. Done when its own acceptance checks pass.
9. Run the checkpoint exactly once, however the run ends: after step 8's
   acceptance checks, before step 10 when that step applies, and before
   `/finalize-changes` prepares the
   landing commit, so a Plan it produces is an ordinary new worktree file riding
   the same squash with no landing-commit join. Every terminal path of the run
   reaches this step before the run ends: a `none-available` or other claim
   stop, a deferral, a blocker, or a refused approval comes straight here from
   wherever it stopped, while an implemented run keeps the position above.

   Main never performs the review itself; it dispatches it per
   [run-checkpoint.md](references/run-checkpoint.md) `## Dispatch`. Done when both
   follow-up lines are recorded per run-checkpoint.md.
10. Only after implementation is accepted and verified, or after the user
   explicitly authorizes rejection, exit the held claim before landing-commit
   creation: main runs, as its own shell call from the PowerShell tool,
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
   with no arguments for completion, appending `-Reject` only after explicit
   user-authorized rejection.

   Success removes only direct-child dependency edges, deletes the selected Plan
   in the worktree, reports the changed paths the landing commit must contain,
   and returns `nextAction: finalize-changes`. That deletion is not reviewed
   before the landing gate, per the
   [landing acceptance table](../finalize-changes/references/landing-acceptance-table.md)
   `## Acceptance table`.
   Done when that result is in hand; the claim stays held until landing
   succeeds, and `/finalize-changes` deletes it after primary advances.
11. End the run per this file's `### IMPORTANT: Session-complete marker`: print
   `SESSION COMPLETE` as the last line only
   when the `/finalize-changes` handoff carried it. Done when the final
   message's last line matches that rule for the run's outcome.

### Post-checkpoint outcomes

| Run outcome | Claim disposition | Next action |
| --- | --- | --- |
| implementation accepted and verified | held | run `Complete-NextPlan.ps1`, then the landing gate |
| changes requested at the `/finalize-changes` landing confirmation | held | prepare only the difference at step 4, whose brief cites the existing preparation snapshot, with its `## In scope` and `## Out of scope`, in place of the deleted claimed Plan; run step 5, then step 8 on top of the unlanded candidate commit, because the request is a choice that came with approval under `### Implementation approval`, so step 7 presents only the difference when step 5 adds a user decision, widens scope, or raises the tier; skip steps 6, 9, and 10, which already ran, and skip the checkpoint for every later outcome of the run; then dispatch a fresh `/finalize-changes` preparation whose brief records that the claim exit already ran |
| user explicitly authorizes rejection | held | run `Complete-NextPlan.ps1 -Reject`, then the landing gate |
| claim wrapper returns `claim.plan-mismatch` | held, identified only by `conflict.heldPlan` | run the checkpoint once, retain the held claim and all work, report requested and held Plans, and stop; never complete, reject, or defer implicitly |
| approval refused, or a blocker whose authoritative result explicitly reports a held claim, with no separate defer instruction | held | run the checkpoint once, retain the claim and all work, report and stop; never complete, reject, or defer implicitly, and any checkpoint work created stays with the retained session |
| blocker before an authoritative claim disposition is available | unknown | follow the supplied `nextAction`, run the checkpoint once, report the error and unknown claim state, and stop, retaining all work with the session; `claim: null` alone never proves claim absence, so add no recovery probe and do not query claim status or mutate a claim |
| user separately and explicitly directs deferral, or the run is already deferred | released/absent only through `Defer-NextPlan.ps1`'s documented result | run the checkpoint once; if it creates tracked follow-up content, use a followup-only landing gate, which runs no Plan completion or rejection and releases no other held claim, otherwise report and stop |
| authoritative result explicitly reports that no claim is held, including `none-available` | absent | run the checkpoint once; if it creates tracked follow-up content, use a followup-only landing gate as the previous row defines it, otherwise report and stop |

## Handoff

The preparation handoff extends the shared form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
with the declared fields below. Main's brief bounds the returned handoff's
verification content to the `Verification` rows; it never asks for a
per-statement enumeration of unaffected results.

- `Claim` — the Plan path, or none; with the resolved state when claimed.
- `Classification` — `Tier 1`, `Tier 2`, or `Tier 3`, and the trigger.
- `Verification` — one row per contradiction, unresolved decision, and verified
  Plan statement whose result requires a card or implementation change, on the
  form `<kind> — <Plan citation> — <result and the change it requires>`, then one
  final row `Unaffected statements: <count>`; a quantitative feasibility
  estimate is not repeated here, and step 4 governs one returned as a `Findings`
  row.
- `Slices` — the only source of the slice partition; the card's `Roles`
  carries role assignments, not the split. One row per disjoint implementation
  slice: the snapshot items it owns, named by section and label or bullet,
  never by their text, and its allowed file scope; one row when the work has
  no disjoint split, and `none` for an empty realized change. Refinements
  accepted from Plan review or grill rounds are written into the preparation
  snapshot;
  every dispatch that changes the snapshot's scope, such as that write-in or a
  redraft, names `Slices` in its `Return:`, in addition to its assigned skill's
  handoff, and returns the field again.

### Execution card presentation/template

The card, cited under `Evidence`, carries the following required content. Main
reads that content and presents its first two sections per
`### Implementation approval`; the card itself is not repeated inline in the
handoff.

```text
Execution card:
### What does this plan do?
<2-4 plain sentences>
### Why this is good for the codebase
<2-4 plain sentences: what prompted it, who uses the result, and how>
- Goal: <result>
- Out of scope: <boundary>
- Tier trigger: <trigger or none>
- Interfaces and invariants: <contracts>
- Acceptance checks: <check and expected observation>
- Roles: <required and conditional assignments>
```

A bullet field's label may carry a parenthetical qualifier before the colon,
and its content may follow the colon or sit in indented lines nested directly
under the bullet.

The preparation fills `Roles` from the Change Workflow step and role
assignments its brief supplies, citing the brief as the evidence and never
reading the workflow itself. When the classified tier differs from the tier
those assignments assume, main rewrites the card's `Roles` field to the
classified tier before the Plan review step.

### Implementation approval

Preparation and claim do not require approval. Before implementation, present
only the execution card's `### What does this plan do?` and
`### Why this is good for the codebase` sections, plus a `### Plan alternatives`
section when `/plan-alternatives` presents candidates (that skill's
`### User presentation`), carrying that skill's comparison and question so it
is clear that choosing one replaces the drafted plan, plus a
`### User decisions needed` section when any decision is unresolved, listing
each one with its options, trade-offs, and a recommendation; the rest of the
resolved Plan and card stays in the snapshot for the reviews and is not
presented. Deliver that presentation per the `### User Interaction` rules in
[`.agents/references/change-workflow.md`](../../references/change-workflow.md) —
on Codex as exactly one complete `<proposed_plan>` block, then ending the turn
without an approval question; on Claude Code, OpenCode, and every other host as
rendered message text whose approval question is the last thing before the
`Follow-up Plans created:` footer, after which the user's next message is the
decision. An approval accepts the recommendation for every listed alternative
or decision it leaves unanswered, except a recommendation to reject the Plan,
which still needs an explicit answer. A chosen alternative returns the run to
preparation and Plan review. When approval came with the choice, the redraft
goes to implementation unless those reviews add a user decision, widen scope
beyond the choice, or raise the tier; in that case, and when the choice came
without approval, main presents only the difference. When the approved presentation differs from the
execution card, main updates the card to the approved scope before the run
continues.

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
- Which reviewer runs at which tier is the Plan review step of
  [change-workflow.md](../../references/change-workflow.md); Tier 3
  additionally follows [tier3-workflow.md](references/tier3-workflow.md).
  Missing a mandatory reviewer blocks. When the preparation handoff reports an
  empty realized change, or that the problem the Plan describes is gone, the
  run goes from preparation straight to this file's
  `### Implementation approval` route, at any tier, without reaching the Plan
  review step. Approving either counts as implementation accepted; once step
  8's acceptance checks pass on the empty change, step 10's completion
  applies. Refusing either is the approval-refused
  `### Post-checkpoint outcomes` row.
- When the claimed Plan carries a `## Sweep` section, main runs `/sweep` on it
  in place of steps 4-8 — preparation, `/plan-alternatives`, Plan review, the
  final claim refresh, the approval presentation, and implementation under the
  Change Workflow; the `/next-plan` request is the approval. Steps 1-3 and 9-11
  run as written, with `/sweep`'s stage close standing in for step 8's
  acceptance checks.
- A Plan, card, scope, invariant, or acceptance change after approval, other
  than the redraft `### Implementation approval` allows, requires a new
  presentation of the difference.
- Deferral uses
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Defer-NextPlan.ps1`
  and only an ordinary live claim. After final preparation has run, deferral
  requires an explicit user instruction given in the current session, recorded
  in the handoff; nothing else unlocks it. Deferral never touches the worktree,
  so uncommitted implementation work stays exactly as it is.
- When preparation or a harness run shows that a runtime acceptance criterion of
  the claimed Plan can be settled only by reading values off rendered pixels,
  because no harness query exposes them, the pixel-evidence prohibition in
  `/agent-harness` `## Handoff` binds main too: never settle it from a
  screenshot yourself. Report the gap and, on the explicit user deferral
  instruction the bullet above requires, act in this order: file the missing
  query as a follow-up Plan through `/create-follow-up-plans`, citing the
  harness command documentation that exposes no such query and naming the
  values the client already computes and the narrowest query that would expose
  them; add that Plan's normalized path to the claimed Plan's `dependsOn`
  through that skill's existing-Plan dependency-change case; then defer. That
  Plan and the `dependsOn` edit are already in the worktree when the checkpoint
  runs and are the followup-only landing gate's content under the deferred row
  in `### Post-checkpoint outcomes`. Deferral is not waiver: the criterion
  stays unmet and the Plan stays unimplemented in the tree.
- Resuming retained work needs an explicit user resume instruction given in the
  current session, recorded in the handoff, and then the targeted claim with
  `-ResumeRetained` appended:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/example.md' -ResumeRetained`
  That switch is valid only with `-Plan` and changes no file. A retained-work
  resume is the one exception to step 1's clean wrapper-created worktree.
- A targeted `none-available` result naming another session's claim keeps its
  `nextAction`: report the result's `holder`
  ([claim-results.md](references/claim-results.md)) and that worktree's state,
  read only with
  `git --no-optional-locks -C <holder worktree> status --porcelain` plus a
  read-only check whether its HEAD is already on the primary branch, never a
  plain `git status`. Taking the Plan over needs an explicit user takeover
  instruction given in the current session, recorded in the handoff, and then
  the targeted claim with `-UserAuthorizedTakeover` appended:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan '<path>' -UserAuthorizedTakeover`
  That switch is valid only with `-Plan`; nothing else releases another
  session's claim.
- On Claude, the checkpoint review covers friction, oversized-result telemetry,
  and isolation evidence after the previous checkpoint's dispatch in the same
  transcript, or from transcript start when none exists, through immediately
  before its own dispatch, excluding only the previous checkpoint's correlated
  completed handoff; `/next-plan-review` covers the rest after landing. Codex
  runs no live checkpoint lens and leaves all three concerns to
  `/next-plan-review`. OpenCode runs no live checkpoint lens and records the
  explicit unsupported state that [run-checkpoint.md](references/run-checkpoint.md)
  `## Measurement states` defines; its landed retrospective is also unsupported
  until an OpenCode transcript source is implemented.
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
