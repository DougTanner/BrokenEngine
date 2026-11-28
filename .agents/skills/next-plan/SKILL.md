---
name: next-plan
description: Validates and deterministically claims one Git-backed executable Plan through WorktreeCli, resolves it against current code, presents the `/plan-alternatives` candidates before the Plan reviews, and presents the execution card's summary, the Plan review changes, and unresolved decisions for implementation approval. Use only when the latest user request explicitly asks to execute `/next-plan` or `$next-plan` and claim a Plan.
disable-model-invocation: true
argument-hint: "[Documents/Plans/... | Projects/Coretide/Documents/Plans/... | partial pattern]"
allowed-tools: [Read, Write, Grep, Glob, Agent, Edit, PowerShell, Skill, AskUserQuestion]
---

# Next Plan

## Purpose

WorktreeCli alone validates metadata, selects, claims, prepares final state, and
releases claims; `Documents/Features` is never scheduler input. The whole Change
Workflow in [change-workflow.md](../../references/change-workflow.md) owns the
cross-skill stage order, and `/finalize-changes` the landing confirmation.

## When to use

Only when the latest user request explicitly asks to execute `/next-plan` or
`$next-plan` and claim a Plan. An audit, inspection, explanation, or quoted
mention of either name does not invoke this skill.

## Inputs

The `argument-hint` value selects the Plan:

- Bare invocation selects the newest eligible Plan by immutable `createdUtc`,
  then normalized UTF-8 path.
- A normalized `Documents/Plans/...` or `Projects/Coretide/Documents/Plans/...`
  argument selects that Plan.
- Any other argument is a case-sensitive partial match against executable Plan
  paths visible to the current CLI (relative to either Plans root); one match
  selects it, and zero or multiple matches block.

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
   Its only write is fetching the session Coretide clone, and it reports a
   bounded point-in-time projection — widen with `-Top <n>` only when a
   decision needs more. For a tier-constrained request, read its result
   through [claim-results.md](references/claim-results.md)
   `## Reading the queue listing`. A bare or plain named-Plan invocation skips
   this step, because step 3 selects and reports every stop itself. Done when
   this file's `## Inputs` resolves exactly one Plan or blocks.
3. Claim that selection as its own shell call, in the root AGENTS.md canonical
   invocation form — bare:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
   or with the requested normalized path or partial pattern quoted after
   `-Plan`, for example:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/example.md'`
   Route targeted `agentCliLock` to
   [claim-exceptions.md](references/claim-exceptions.md#agent-cli-override)
   before `nextAction`; read any other non-`prepare` result via
   [claim-results.md](references/claim-results.md) `## nextAction` (`prepare`
   needs no read).
   Done when the result reports `nextAction: prepare`.
4. Dispatch one preparation `planner` to verify the Plan against current code.

   Pass `Skill: /prepare-change`, `mode: claimed-plan`, `Return:` by citation,
   and [prepare-change/SKILL.md](../prepare-change/SKILL.md) `## Inputs` in the
   [subagent-reporting.md](../../references/subagent-reporting.md)
   `## Task brief` form, reading for workflow references only those two
   sections and, when any claim result of this run carried `sync`,
   [claim-results.md](references/claim-results.md)
   `## Session baseline and the sync object` (to its first `###`).

   The Plan is immutable, current code wins, and every delegation states that
   Plan and card statements are hypotheses, so every contradiction returns to
   main as a card correction rather than an edit.

   When that handoff returns `Status: BLOCKED` reporting an unconfirmed root
   cause in `Residuals`:

   - main dispatches one `implementer` for `/external-diagnose-bug`, on a
     brief citing the recorded baseline and `claim.sections` line ranges from
     `Invoke-NextPlanClaim.ps1` for the claimed Plan's investigation portion
     (`Documents/Plans/AGENTS.md` `## Plan files`), none of which main reads;
   - a proven root cause goes to a fresh `/prepare-change`
     (`mode: claimed-plan`) `planner` whose
     [subagent-handoff.md](../../references/subagent-handoff.md)
     `## Continuation capsule` carries the diagnosis handoff; that handoff
     feeds the rest of this step;
   - no proven root cause is a blocker main reports to the user under the
     approval-refused row of `### Post-checkpoint outcomes`, with the claim
     held.

   After that handoff, before Plan review, run
   [plan-alternatives/SKILL.md](../plan-alternatives/SKILL.md) when its trigger
   fires and present per `### User presentation`; step 5 runs on the user's
   pick, after redrafting any non-drafted pick.

   `/plan-alternatives` runs and is presented once per run: a re-entry of this
   step keeps the earlier pick and does not present alternatives again.

   Done when the handoff cites the snapshot by `##` section selectors under
   `Evidence`, its `Decisive checks` row reports both `headings` values `true`
   and `card.missingFields` empty, and, when that presentation ran, its pick has
   arrived and a non-drafted pick is redrafted or routed to rejection.
5. Run [change-workflow.md](../../references/change-workflow.md)
   `#### Step 3 — Plan review` after preparation and alternatives and before the
   final claim refresh; Tier 3 adds
   [tier3-workflow.md](references/tier3-workflow.md) `## Plan review`.

   Done when every required review completes and accepted findings resolve, or
   main reports a missing mandatory reviewer as a blocker.
6. Unless step 10 already ran (the changes-requested row in
   `### Post-checkpoint outcomes`), after the pre-claim join in
   [change-workflow.md](../../references/change-workflow.md)
   `#### Step 3 — Plan review`, invoke step 3's `Invoke-NextPlanClaim.ps1`
   idempotently with `-Plan '<claimed Plan path>'`, never bare, immediately
   before the final preparation handoff: another session's `plan validate` or
   `plan claim-next` can release this session's claim, and a bare run would then
   claim a different Plan the user never confirmed.

   On a `claim.plan-moved` result, main asks the user whether to claim
   `moved.to`; a yes reruns step 3's claim with `-Plan '<moved.to>'`, from then
   on the claimed Plan path, and continues this step; a no ends the run on the
   `### Post-checkpoint outcomes` row for an authoritative result that
   explicitly reports no claim is held.

   Find the sync range by reading only
   [claim-results.md](references/claim-results.md) `## Final-refresh sync brief`
   and `## Session baseline and the sync object` (to its first `###`), applying
   both; when a range exists, never list or read it, and dispatch one
   `researcher` on that brief.

   When any row of that `researcher`'s handoff reports a touch, return to step
   4, rerun the affected Plan review checks in step 5, and repeat this final
   refresh before approval; reuse review evidence whose inputs did not change.

   Until step 8 starts, main repeats this final refresh for a primary advance
   it learns of after it, and the two paragraphs above apply to that refresh's
   sync range; once step 8 starts, main leaves the advance to
   `/finalize-changes`, which reconciles it at landing.

   Done when the claim result reports the held claim and either this step has
   no sync range or no row reports a touch.
7. Present for approval per this file's `### Implementation approval`, unless
   that section sends the run to implementation on an earlier user choice. Done
   when the user's decision arrives or that earlier choice stands.
8. Implement the approved change. After the Implement and propagate step's
   pre-slice gate in [change-workflow.md](../../references/change-workflow.md)
   runs any remaining or stale pre-edit checks (using step 6's recorded
   baseline in [claim-results.md](references/claim-results.md)
   `## Session baseline and the sync object`, under `/verify-acceptance`'s
   changed-inputs rule), main dispatches one `/implement-plan` `implementer`
   per `Slices` row of the latest handoff that returned that field, each brief
   citing as the approved plan the step-4 preparation snapshot (the `Temp/`
   path whose `## In scope` and `## Out of scope` the preparation handoff
   cites, or the difference preparation's snapshot on the changes-requested or
   `completion.plan-cited` route) and copying that row as the assigned items
   and allowed file scope; main never reads the snapshot's `## In scope`
   itself. Main assigns each change the user approved after the plan to exactly
   one row, adding one slice when none fits, and passes `none` for those
   changes to every other row. `Slices: none` dispatches no implementer. When
   step 4's split question is answered no, including one accepted as its
   recommendation, no stage of the claimed Plan lands mid-run while the claim
   stays held: main runs the stages in the order the stage-split entry lists
   them, dispatching a stage's `Slices` rows only after the previous stage's
   acceptance checks pass; each stage but the last completes without landing,
   through its own acceptance checks and the Change Workflow Verify the
   acceptance table step in change-workflow.md, and step 10's one claim exit
   and one `/finalize-changes` landing carry every stage. Done when the last
   stage's acceptance checks pass; a change kept as one stage is its own last
   stage.
9. Run the checkpoint exactly once, however the run ends, per
   [run-checkpoint.md](references/run-checkpoint.md) `## Dispatch` (overlap
   gate and pre-step-10 / pre-preparation join). Main never performs the review
   itself. Done when both follow-up lines are recorded per that file's
   `## Measurement states` and `## Follow-up routing`.
10. Only after implementation is accepted and verified, or after the user
   explicitly authorizes rejection, exit the held claim before landing-commit
   creation: main runs, as its own shell call from the PowerShell tool,
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
   with no arguments for completion, appending `-Reject` only after explicit
   user-authorized rejection.

   What that result reports and changes is in
   [claim-results.md](references/claim-results.md)
   `## Claim-exit result fields`. Main passes that result verbatim into every
   `/finalize-changes` brief the run dispatches, after acceptance, after
   rejection, and after changes are requested at the landing confirmation.

   Done when the success result is in hand; the claim stays held until landing
   succeeds, and `/finalize-changes` deletes it after primary advances.
11. End the run per this file's `### IMPORTANT: Session-complete marker`. Done
   when the final message's ending matches that rule for the run's outcome.

### Post-checkpoint outcomes

| Run outcome | Claim disposition | Next action |
| --- | --- | --- |
| implementation accepted and verified | held | run `Complete-NextPlan.ps1`, then the landing gate |
| changes requested at the `/finalize-changes` landing confirmation | held | follow [claim-exceptions.md](references/claim-exceptions.md) `## Changes requested at landing` |
| user explicitly authorizes rejection | held | run `Complete-NextPlan.ps1 -Reject`, then the landing gate |
| `Complete-NextPlan.ps1` refuses with `completion.plan-cited` | held | run step 4's difference preparation per [`../prepare-change/SKILL.md`](../prepare-change/SKILL.md) `## Inputs` (`mode: claimed-plan`), step 5, and step 8, skipping the checkpoint for every later outcome of the run; step 4's citing-line rule puts each rewrite on the card and into `Slices` as scope that rule already approves, which needs no presentation, and step 7 presents per `### Implementation approval`; skip steps 6 and 9; then run step 10 again with the same arguments |
| claim wrapper returns `claim.plan-mismatch` | held, identified only by `conflict.heldPlan` | run the checkpoint once, retain the held claim and all work, report requested and held Plans, and stop; never complete, reject, or defer implicitly |
| approval refused, or a blocker other than `completion.plan-cited` whose authoritative result explicitly reports a held claim, with no separate defer instruction | held | run the checkpoint once, retain the claim and all work, report and stop; never complete, reject, or defer implicitly, and any checkpoint work created stays with the retained session |
| blocker before an authoritative claim disposition is available | unknown | follow the supplied `nextAction`, run the checkpoint once, report the error and unknown claim state, and stop, retaining all work with the session; `claim: null` alone never proves claim absence, so add no recovery probe and do not query claim status or mutate a claim |
| user separately and explicitly directs deferral, or run is already deferred | released/absent only via `Defer-NextPlan.ps1`'s result | run the checkpoint once; if deferral or checkpoint created tracked follow-up content, use a followup-only landing gate (runs no Plan completion or rejection and releases no other held claim), else report and stop |
| authoritative result explicitly reports that no claim is held, including `none-available` | absent | run the checkpoint once; if it creates tracked follow-up content, use a followup-only landing gate as the previous row defines it, otherwise report and stop |

## Handoff

### Implementation approval

Preparation and claim do not require approval. Before implementation that no
earlier user choice approved (below), present only `Approval summary` from the
latest card-emitting `planner` handoff (without reading `## Execution card`),
plus a `### Plan review changes` section listing each change the Plan review
step made to the approach being approved, or `none`, plus a
`### User decisions needed` section when any decision is unresolved, listing
each one with its options, trade-offs, and a recommendation; the alternatives
the `/plan-alternatives` presentation already showed are not presented again,
and the full Plan and card stay in the snapshot for the reviews and are not
presented. Deliver that presentation per the `### User Interaction` rules in
[`.agents/references/change-workflow.md`](../../references/change-workflow.md) —
on Codex as exactly one complete `<proposed_plan>` block, then ending the turn
without an approval question; on Claude Code and every other host as rendered
message text whose approval question is the last thing in the message, after
which the user's next message is the decision. An approval accepts the
recommendation for every decision it leaves unanswered, except a recommendation
to reject the Plan, which still needs an explicit answer. A different approach
the user chooses at approval returns the run to preparation and Plan review.
Every user choice — the `/plan-alternatives` pick, an `/external-grill-plan`
answer, a choice at approval, or a change request at the `/finalize-changes`
landing confirmation — approves implementation unless it rejects the Plan,
refuses approval, or defers it from step 4's size entry. A choice approves only
if every non-`none` entry on the current `### Plan review changes` list was
shown to the user before the choice was given, so a choice given before an
entry existed, such as the step 4 `/plan-alternatives` pick, does not approve
that entry. Otherwise main presents the entries the user has not seen, and the
user's next choice decides. Separately, even when every entry was shown, main
presents every question for the user that is still unanswered before
implementation. Main presents entries in the `### Plan review changes` form and
questions in the `### User decisions needed` form, together and delivered as
above. Whether a reply answered a decision follows
[`authority-order.md`](../../references/authority-order.md). When the approved
presentation differs from the execution card, main dispatches, before step 8,
one fresh preparation `planner` whose continuation capsule
([`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
`## Continuation capsule`) carries the snapshot step 8 cites as the approved
plan with its `##` selectors, and the user's approval answers; that worker
updates the card's affected fields to the approved scope and refreshes
`Pre-edit checks`, `Harness checks`, and `Approval summary`; the dispatch
always names `Slices` in its `Return:`, whether or not the update changes the
snapshot's scope, so step 8 dispatches from that handoff's `Slices`. Main does
not read or edit the card or the snapshot for this update. A yes to step 4's
split question, including one accepted as its recommendation, is the user's
direction to defer stages 2 to M. The preparation `planner` that records the yes
— that approval-update worker, or the preparation `planner` of the Tier-3 grill
round that carries the yes — also marks each card acceptance check and citation
rewrite of stages 2 to M deferred to its stage's Plan, never deleting it, and
names those stages in the card's `Out of scope`. After the handoff that records
the yes and before step 8, and never earlier, because `/create-follow-up-plans`
rejects in-scope work, main sends stages 2 to M, as leftovers the user chose to
defer, through the Change Workflow Verify the acceptance table step's Leftovers
rule, with a brief citing as evidence the claimed Plan path and the snapshot
path with its `## Unresolved decisions`, `## Execution card`, `## Decisions`,
and `## Design` selectors, citing the claimed Plan's matching section for either
of the last two the snapshot lacks, and stating that each stage Plan restates in
its own words the decisions and design its stage needs, and that the dispatch
also performs the citation rewrites the card marks deferred to each stage,
renaming each such pending claim in its citing Plan to that stage Plan's file
name.

### IMPORTANT: Session-complete marker

The user decides on this line alone that the run is over, so it must be
unambiguous. When, and only when, the `/finalize-changes` handoff carries its
`SESSION COMPLETE` line, whose conditions that skill's `## Handoff` owns,
main's final message of that turn ends with the bare line `SESSION COMPLETE`
followed by the bare last line `/reset-worktree?` (order: that skill's
`## When to use`), and asks the user nothing else. Every other ending —
retained claim, blocker, refused approval, deferral, unknown claim state, or
work still to land — prints neither line.

## Rules

- Act per `nextAction`, never reconstructing script transitions, except
  targeted `agentCliLock` (route to
  [claim-exceptions.md](references/claim-exceptions.md#agent-cli-override)
  before `nextAction`) and step 6's confirmed `claim.plan-moved` route
  (replaces `stop-report-to-user`). Never select or claim another candidate on
  a `-Plan` run outside `claim.plan-moved`.
- Never create or adopt a worktree, or inspect machine-local claims directly.
- When preparation reports an empty realized change or that the Plan's problem
  is gone, go straight to `### Implementation approval` at any tier, skipping
  Plan review. Approving either counts as implementation accepted (step 8 empty
  checks then step 10 completion); refusing is the approval-refused
  `### Post-checkpoint outcomes` row.
- A Plan, card, scope, invariant, or acceptance change after approval requires
  a new presentation of the difference, unless `### Implementation approval`
  or the `completion.plan-cited` row of `### Post-checkpoint outcomes` sends it
  to implementation without one.
- Deferral (after final preparation), pixel-evidence deferral, retained-work
  resume, takeover, and agent CLI override each need an explicit
  current-session user instruction recorded in the handoff; follow
  [claim-exceptions.md](references/claim-exceptions.md) for them and targeted
  `none-available` `holder`/`agentCliLock` results.

## References

- [references/claim-results.md](references/claim-results.md) — how each claim,
  listing, and claim-exit result is read.
- [references/claim-exceptions.md](references/claim-exceptions.md) — rare claim
  branches (deferrals, retained resume, takeover, agent CLI override, landing
  changes).
- [`../prepare-change/SKILL.md`](../prepare-change/SKILL.md) — the step-4
  preparation `planner` contract (`mode: claimed-plan`).
- [references/tier3-workflow.md](references/tier3-workflow.md) — the additional
  Tier-3 preparation route.
- [references/run-checkpoint.md](references/run-checkpoint.md) — the end-of-run
  checkpoint mechanics.
- [references/follow-up-provenance.md](references/follow-up-provenance.md) —
  which session ID applies and the provenance block.
