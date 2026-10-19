---
name: next-plan
description: Validates and deterministically claims one Git-backed Documents/Plans Plan through WorktreeCli, resolves it against current code, presents the `/plan-alternatives` candidates before the Plan reviews, and presents the execution card's summary, the Plan review changes, and unresolved decisions for implementation approval. Use only when the latest user request explicitly asks to execute `/next-plan` or `$next-plan` and claim a Plan.
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
   exactly one handoff contract binds the worker.

   It also carries the Change Workflow step and role assignments the claimed
   Plan triggers, as main states them from the workflow, and cites
   [`references/preparation-brief.md`](references/preparation-brief.md) by path
   for the snapshot, the leftovers, the citation rule for citing Plans, and the
   stage split.

   The Plan is immutable, current code wins, and every delegation states that
   Plan and card statements are hypotheses, so every contradiction returns to
   main as a card correction rather than an edit.

   Main writes that brief from the claimed Plan's own citations — path plus
   line range — citing the claimed Plan as its path plus the section line
   ranges a `^## ` heading Grep of it returns.

   Main reads only the Plan sections it must state from, such as the risk tier
   that sets the role assignments, just as it reads only the cited
   `## Task brief` section above.

   It leaves reading both the target source and the handoff and reporting
   references the brief cites to the dispatched `implementer`, naming those
   references as paths only, and reads source itself only for a decision it
   must make that the Plan and the returned handoff cannot settle.

   The brief bounds the card's verification evidence: no verbatim source
   text, except an acceptance item whose purpose is proposed replacement
   text.

   A preparation handoff that returns a quantitative feasibility estimate as a
   Finding must name the check proving that the measured trial preserved the
   source document's rule or item inventory, or report the estimate as
   unvalidated. That is an expectation of the dispatch, not a gate the Done
   condition tests.

   After that preparation handoff, and before the Plan review reviewers, main
   runs `/plan-alternatives` when its trigger fires and presents per that
   skill's `### User presentation`; step 5 runs only on the approach the user
   picks, after the redraft a pick other than the drafted plan requires.

   `/plan-alternatives` runs and is presented once per run: a re-entry of this
   step keeps the earlier pick and does not present alternatives again.

   Done when the handoff cites the snapshot by the selectors
   preparation-brief.md requires, its `Decisive checks` row reports both
   `headings` values `true` and `card.missingFields` empty, and, when that
   presentation ran, its pick has
   arrived and a pick other than the drafted plan is redrafted or routed to
   rejection.
5. Run the Plan review step in
   [change-workflow.md](../../references/change-workflow.md), after preparation and alternatives and
   before the final claim refresh. Tier 3 retains the additional route in
   [tier3-workflow.md](references/tier3-workflow.md). Done when every required review has
   completed and accepted findings are resolved, or a missing mandatory
   reviewer is reported as a blocker.
6. Unless step 10 already ran (the changes-requested row in
   `### Post-checkpoint outcomes`), invoke step 3's claim script idempotently
   immediately before the final preparation handoff.

   When that result carries a `sync` object, the tree moved under the
   preparation evidence: main never lists or reads that range itself, and
   dispatches one `researcher` on the sync brief in
   [claim-results.md](references/claim-results.md)
   `## Session baseline and the sync object`.

   When any row of that `researcher`'s handoff reports a touch, return to step 4, rerun the affected Plan
   review checks in step 5, and repeat this final refresh before approval;
   reuse review evidence whose inputs did not change.

   Done when the claim result reports the held claim and either the claim
   result carries no `sync` object or no row reports a touch.
7. Present for approval per this file's `### Implementation approval`, unless
   that section sends the run to implementation on an earlier user choice. Done
   when the user's decision arrives or that earlier choice stands.
8. Implement the approved change. A pre-edit acceptance check whose brief
   `Baseline:` differs from the recorded session baseline
   ([claim-results.md](references/claim-results.md)
   `## Session baseline and the sync object`) is stale and runs again under the
   changed-inputs re-run rule `/verify-acceptance` applies. After the pre-edit
   acceptance checks the Change Workflow Implement and propagate step in
   [change-workflow.md](../../references/change-workflow.md) orders, main
   dispatches one `/implement-plan` `implementer` per `Slices` row of the
   latest handoff that returned that field, each brief citing as the approved
   plan the step-4 preparation snapshot (the `Temp/` path whose `## In scope`
   and `## Out of scope` the preparation handoff cites, or the difference
   preparation's snapshot on the changes-requested or `completion.plan-cited`
   route) and copying that row
   as the assigned items and allowed file scope; main never reads the
   snapshot's `## In scope` itself. Main assigns each change the user approved
   after the plan to exactly one row, adding one slice when none fits, and
   passes `none` for those changes to every other row. `Slices: none`
   dispatches no implementer. Done when its own acceptance checks pass.
9. Run the checkpoint exactly once, however the run ends: after step 8's
   acceptance checks, before step 10 when that step applies, and before
   `/finalize-changes` prepares the
   landing commit, so a Plan it produces is an ordinary new worktree file riding
   the same squash with no landing-commit join.

   Main never performs the review itself; it dispatches it per
   [run-checkpoint.md](references/run-checkpoint.md) `## Dispatch`. Done when both
   follow-up lines are recorded per run-checkpoint.md.
10. Only after implementation is accepted and verified, or after the user
   explicitly authorizes rejection, exit the held claim before landing-commit
   creation: main runs, as its own shell call from the PowerShell tool,
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
   with no arguments for completion, appending `-Reject` only after explicit
   user-authorized rejection.

   What that result reports and changes is in
   [claim-results.md](references/claim-results.md) `## Claim-exit result fields`.
   Main passes that result verbatim into every `/finalize-changes` brief the run
   dispatches, after acceptance, after rejection, and after changes are
   requested at the landing confirmation.

   Done when the success result is in hand; the claim stays held until landing
   succeeds, and `/finalize-changes` deletes it after primary advances.
11. End the run per this file's `### IMPORTANT: Session-complete marker`: print
   `SESSION COMPLETE` as the last line only
   when the `/finalize-changes` handoff carried it. Done when the final
   message's last line matches that rule for the run's outcome.

### Post-checkpoint outcomes

| Run outcome | Claim disposition | Next action |
| --- | --- | --- |
| implementation accepted and verified | held | run `Complete-NextPlan.ps1`, then the landing gate |
| changes requested at the `/finalize-changes` landing confirmation | held | follow [claim-exceptions.md](references/claim-exceptions.md) `## Changes requested at landing` |
| user explicitly authorizes rejection | held | run `Complete-NextPlan.ps1 -Reject`, then the landing gate |
| `Complete-NextPlan.ps1` refuses with `completion.plan-cited` | held | run the changes-requested row's difference preparation, step 5, and step 8, skipping the checkpoint for every later outcome of the run, except that the step-4 brief also states whether the refused exit was completion or `-Reject`, so step 4's citing-line rule puts each rewrite on the card and into `Slices` as scope that rule already approves, which needs no presentation of the difference, and step 7 presents only a user decision that preparation returns; skip steps 6 and 9; then run step 10 again with the same arguments |
| claim wrapper returns `claim.plan-mismatch` | held, identified only by `conflict.heldPlan` | run the checkpoint once, retain the held claim and all work, report requested and held Plans, and stop; never complete, reject, or defer implicitly |
| approval refused, or a blocker other than `completion.plan-cited` whose authoritative result explicitly reports a held claim, with no separate defer instruction | held | run the checkpoint once, retain the claim and all work, report and stop; never complete, reject, or defer implicitly, and any checkpoint work created stays with the retained session |
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
- `Verification` — one row per unresolved decision, contradiction needing a
  user decision or override, problem found gone or empty realized change,
  scope widened beyond the Plan's `## In scope`, and dropped or weakened Plan
  acceptance criterion or invariant, on the form
  `<kind> — <Plan citation> — <result and the change it requires>`; then one
  row `Snapshot corrections: <count>` for every other verified Plan statement
  whose result requires a card or implementation change, each written in the
  snapshot's corrections section, which `Evidence` cites; then one final row
  `Unaffected statements: <count>`; a quantitative feasibility estimate is not
  repeated here, and step 4 governs one returned as a `Findings` row.
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
- `Candidate zero` — the drafted mechanism as the `Mechanism` through
  `Pays off when` rows exactly as
  [`../plan-alternatives/SKILL.md`](../plan-alternatives/SKILL.md)
  `## Handoff` declares them, required inline in the handoff; `Mechanism` is
  written to satisfy the brief exclusions of the `/plan-alternatives` bullet in
  [`../../references/change-workflow.md`](../../references/change-workflow.md)
  `#### Step 2 — Prepare and explore alternatives` and names neither the plan
  file nor its `Temp/` snapshot;
  `none` for an empty realized change and for every preparation handoff after
  the `/plan-alternatives` pick.

### Execution card

The card, on the template in
[`../../references/execution-card.md`](../../references/execution-card.md)
`## Template`, is cited under `Evidence` and not repeated inline in the
handoff; main presents its first two sections per `### Implementation approval`.
When the classified tier differs from the tier the brief's role assignments
assume, main rewrites the card's `Roles` field to the classified tier before the
Plan review step.

### Implementation approval

Preparation and claim do not require approval. Before implementation that no
earlier user choice approved (below), present only the execution card's
`### What does this plan do?` and
`### Why this is good for the codebase` sections, plus a
`### Plan review changes` section listing each change the Plan review step
made to the approach being approved, or `none`, plus a
`### User decisions needed` section when any decision is unresolved, listing
each one with its options, trade-offs, and a recommendation; the alternatives
the `/plan-alternatives` presentation already showed are not presented again,
and the rest of the resolved Plan and card stays in the snapshot for the
reviews and is not presented. Deliver that presentation per the `### User Interaction` rules in
[`.agents/references/change-workflow.md`](../../references/change-workflow.md) —
on Codex as exactly one complete `<proposed_plan>` block, then ending the turn
without an approval question; on Claude Code, OpenCode, and every other host as
rendered message text whose approval question is the last thing before the
`Follow-up Plans created:` footer, after which the user's next message is the
decision. An approval accepts the recommendation for every decision it leaves
unanswered, except a recommendation to reject the Plan, which still needs an
explicit answer. A different approach the user chooses at approval returns the
run to preparation and Plan review. Every user choice in the run that neither
rejects the Plan, refuses approval, nor defers it from step 4's size entry —
the `/plan-alternatives` pick, an
`/external-grill-plan` answer, a choice at approval, or a change request at the
`/finalize-changes` landing confirmation — approves implementation, and the run
goes to implementation unless an existing or new question for the user remains
that the user has not yet answered; main then presents only those questions, in
the `### User decisions needed` form and delivered as above. Whether a reply
answered a decision follows
[`authority-order.md`](../../references/authority-order.md). When the approved
presentation differs from the execution card, main dispatches, before step 8,
one fresh preparation `implementer` whose continuation capsule
([`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
`## Continuation capsule`) carries the snapshot step 8 cites as the approved
plan with its `##` selectors, and the user's approval
answers; that worker updates the card's affected fields to the approved scope,
and the dispatch always names `Slices` in its `Return:`, whether or not the
update changes the snapshot's scope, so step 8 dispatches from that handoff's
`Slices`. Main does not read or edit the card or the snapshot for this update.
A yes to step 4's split question, including one accepted as its
recommendation, is the user's direction to defer stages 2 to M. The
preparation `implementer` that records the yes — that approval-update worker,
or the preparation `implementer` of the Tier-3 grill round that carries the
yes — also marks each card acceptance check and citation rewrite of stages 2
to M deferred to its stage's Plan, never deleting it, and names those stages
in the card's `Out of scope`. After the handoff that records the yes and
before step 8, and never earlier, because `/create-follow-up-plans` rejects
in-scope work, main sends stages 2 to M, as leftovers the user chose to defer,
through the Change Workflow Verify the acceptance table step's Leftovers rule,
with a brief citing as evidence the claimed Plan path and the snapshot path
with its `## Unresolved decisions`, `## Execution card`, `## Decisions`, and
`## Design` selectors, citing the claimed Plan's matching section for either
of the last two the snapshot lacks, and stating that each stage Plan restates
in its own words the decisions and design its stage needs, and that the
dispatch also performs the citation rewrites the card marks deferred to each
stage, renaming each such pending claim in its citing Plan to that stage
Plan's file name.

### IMPORTANT: Session-complete marker

The user closes the session tab on this line alone, so it must be
unambiguous. When, and only when, the `/finalize-changes` handoff carries its
`SESSION COMPLETE` line, whose conditions that skill's `## Handoff` owns,
main's final message of that turn ends with the bare line `SESSION COMPLETE` as
its very last line, after the `Follow-up Plans created:` footer, and asks the
user nothing. Every other ending — retained claim, blocker, refused approval,
deferral, unknown claim state, or work still to land — never prints that line.

## Rules

- The preparation worker must never run the mutation-capable claim script.
- Every result is acted on per its `nextAction`, never by reconstructing a
  script's transitions, and for a `-Plan`-targeted invocation the manager never
  selects or claims a different candidate in that run.
- Never create or adopt a worktree, or inspect machine-local claims directly.
- When the preparation handoff reports an empty realized change, or that the
  problem the Plan describes is gone, the run goes from preparation straight to
  this file's `### Implementation approval` route, at any tier, without reaching
  the Plan review step. Approving either counts as implementation accepted;
  once step 8's acceptance checks pass on the empty change, step 10's
  completion applies. Refusing either is the approval-refused
  `### Post-checkpoint outcomes` row.
- When the claimed Plan carries a `## Sweep` section, main runs `/sweep` on it
  in place of steps 4-8 — preparation, `/plan-alternatives`, Plan review, the
  final claim refresh, the approval presentation, and implementation under the
  Change Workflow; the `/next-plan` request is the approval. Steps 1-3 and 9-11
  run as written, with `/sweep`'s stage close standing in for step 8's
  acceptance checks.
- A Plan, card, scope, invariant, or acceptance change after approval, other
  than one `### Implementation approval` or the `completion.plan-cited` row of
  `### Post-checkpoint outcomes` sends to implementation without a
  presentation, requires a new presentation of the difference.
- Deferral, pixel-evidence deferral, retained-work resume, and takeover each
  need an explicit user instruction given in the current session (for
  deferral, once final preparation has run), recorded in the handoff; follow
  [claim-exceptions.md](references/claim-exceptions.md) for them, for a runtime
  criterion only rendered pixels can settle, and for a targeted
  `none-available` result naming another session's claim.

## References

- [references/claim-results.md](references/claim-results.md) — how each claim,
  listing, and claim-exit result is read.
- [references/claim-exceptions.md](references/claim-exceptions.md) — deferral,
  pixel-evidence deferral, retained resume, takeover, and changes requested at
  landing.
- [references/preparation-brief.md](references/preparation-brief.md) — the
  step-4 preparation `implementer`'s snapshot, leftovers, citation rule, and
  stage split; cited in its brief, not read by main.
- [references/tier3-workflow.md](references/tier3-workflow.md) — the additional
  Tier-3 preparation route.
- [references/run-checkpoint.md](references/run-checkpoint.md) — the end-of-run
  checkpoint mechanics.
- [references/follow-up-provenance.md](references/follow-up-provenance.md) —
  which session ID applies and the provenance block.
