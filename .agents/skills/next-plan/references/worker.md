# Next Plan Worker

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
   itself. Done when `## Inputs` in [`../SKILL.md`](../SKILL.md) resolves exactly
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
   [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md).

   The Plan is immutable, current code wins, and every delegation states that
   Plan and card statements are hypotheses, so every contradiction returns to
   main as a card correction rather than an edit.

   Main writes that brief from the claimed Plan's own citations — path plus
   line range — and leaves reading the target source to the dispatched
   `implementer`, reading source itself only for a decision it must make that
   the Plan and the returned handoff cannot settle.

   The brief bounds the card's verification evidence: each acceptance item
   comes back as a `path:line` citation plus a verdict, not verbatim source
   text, except an item whose purpose is proposed replacement text. That is an
   expectation of the dispatch, not a gate the Done condition tests.

   After that preparation handoff, and before the Plan review reviewers and the
   step 6 approval presentation, main runs `/plan-alternatives` when its trigger
   fires.

   Done when the execution card carries every field of the card template in
   [`../SKILL.md`](../SKILL.md) `## Handoff`.

   When `/plan-audit` will read a scratch snapshot instead of the claimed Plan path,
   the preparation `implementer` writes the complete resolved Plan—mechanism, corrections,
   exact `## In scope` and `## Out of scope` sections with content intact, and complete
   execution card—and cites its path plus `## Execution card` selector under `Evidence` for `/plan-audit`.
5. Invoke step 3's claim script idempotently immediately before the final
   preparation handoff. When that result carries a `sync` object, the tree
   moved under the preparation evidence: diff `sync.from..sync.to` against the
   paths the preparation handoff cited and re-dispatch step 4 when they
   intersect. Done when it reports the held claim.
6. Present for approval per `### Implementation approval` in
   [`../SKILL.md`](../SKILL.md). Done when the user's decision arrives.
7. Implement the approved change. Done when its own acceptance checks pass.
8. Run the checkpoint exactly once, however the run ends: after step 7's
   acceptance checks, before step 9, and before `/finalize-changes` prepares the
   landing commit, so a Plan it produces is an ordinary new worktree file riding
   the same squash with no landing-commit join. Every terminal path of the run
   reaches this step before the run ends: a `none-available` or other claim
   stop, a deferral, a blocker, or a refused approval comes straight here from
   wherever it stopped, while an implemented run keeps the position above.

   Main never performs either review itself; it dispatches them per
   [run-checkpoint.md](run-checkpoint.md). Done when both follow-up lines are
   recorded per run-checkpoint.md.
9. Exit the claim before landing-commit creation: an `implementer` runs
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Complete-NextPlan.ps1`
   with no arguments for completion, appending `-Reject` only after explicit
   user-authorized rejection.

   Success removes only direct-child dependency edges, deletes the selected Plan
   in the worktree, reports the changed paths the landing commit must contain,
   and returns `nextAction: finalize-changes`. Done when that result is in hand;
   the claim stays held until landing succeeds, and `/finalize-changes` deletes
   it after primary advances.

## Rules

- The preparation worker must never run the mutation-capable claim script.
- Every result is acted on per its `nextAction`, and for a `-Plan`-targeted
  invocation the manager never selects or claims a different candidate in that
  run.
- The context baseline is provisional until a claim reports a `sync` object.
  Never create/adopt a worktree or inspect machine-local claims directly.
- Which reviewer runs at which tier is the Plan review step of root
  [AGENTS.md](../../../../AGENTS.md); Tier 3 additionally follows
  [tier3-workflow.md](tier3-workflow.md). Missing a mandatory reviewer blocks.
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
- The checkpoint review covers friction observable in the transcript up to its
  own dispatch; `/next-plan-review` covers the rest after landing.
- [claim-results.md](claim-results.md) owns how each claim, listing, and
  claim-exit result is read, including the `sync` and `nextAction` fields, the
  listing's snapshot limits and tier-constrained reading procedure, and the
  completion and retained-path result shapes. Do not reconstruct any script's
  transitions.
- [follow-up-provenance.md](follow-up-provenance.md) owns where the provenance
  block's values come from.
