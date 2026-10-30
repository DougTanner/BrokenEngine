# Claim Exceptions

Read this reference when a `/next-plan` run meets one of the rare branches
below: a deferral, a runtime criterion only rendered pixels can settle, a
retained-work resume, a targeted `none-available` result naming another
session's claim, or changes requested at the landing confirmation. The workflow
stays in [`/next-plan`](../SKILL.md).

## Deferral

Deferral uses
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Defer-NextPlan.ps1`
and only an ordinary live claim, on the user instruction
[`../SKILL.md`](../SKILL.md) `## Rules` requires; nothing else unlocks it.
Deferral never touches the worktree, so uncommitted implementation work stays
exactly as it is.

A defer reply to step 4's single-stage size entry
([preparation-brief.md](preparation-brief.md) `## Stage split`) is not an
approval: it is that explicit current-session user instruction, and the run
takes this deferral route instead of implementation.

## Pixel-evidence deferral

When preparation or a harness run shows that a runtime acceptance criterion of
the claimed Plan can be settled only by reading values off rendered pixels,
because no harness query exposes them, the pixel-evidence prohibition in
`/agent-harness` `## Handoff` binds main too: never settle it from a screenshot
yourself. Report the gap and, on the explicit user deferral instruction
[`../SKILL.md`](../SKILL.md) `## Rules` requires, act in this order: file the
missing query as a follow-up Plan through `/create-follow-up-plans`, citing the
harness command documentation that exposes no such query and naming the values
the client already computes and the narrowest query that would expose them; add
that Plan's normalized path to the claimed Plan's `dependsOn` through that
skill's existing-Plan dependency-change case; then defer. That Plan and the
`dependsOn` edit are already in the worktree when the checkpoint runs and are
the followup-only landing gate's content under the deferred row in
[post-checkpoint outcomes](../SKILL.md#post-checkpoint-outcomes). Deferral is
not waiver: the criterion stays unmet and the Plan stays unimplemented in the
tree.

## Retained resume

Resuming retained work, on the user instruction [`../SKILL.md`](../SKILL.md)
`## Rules` requires, uses the targeted claim with `-ResumeRetained` appended:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/example.md' -ResumeRetained`
That switch is valid only with `-Plan` and changes no file. A retained-work
resume and the exact `-Plan` dirty-gate exemption in
[claim-results.md](claim-results.md) `## Session baseline and the sync object`
are the two exceptions to [the `/next-plan` steps](../SKILL.md#steps) step 1's
clean wrapper-created worktree.

## Takeover

A targeted `none-available` result naming another session's claim keeps its
`nextAction`: report the result's `holder` ([claim-results.md](claim-results.md))
and that worktree's state, read only with
`git --no-optional-locks -C <holder worktree> status --porcelain` plus a
read-only check whether its HEAD is already on the primary branch, never a
plain `git status`. Taking the Plan over, on the user instruction
[`../SKILL.md`](../SKILL.md) `## Rules` requires, uses the targeted claim with
`-UserAuthorizedTakeover` appended:
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan '<path>' -UserAuthorizedTakeover`
That switch is valid only with `-Plan`; nothing else releases another session's
claim.

## Changes requested at landing

When the user requests changes at the `/finalize-changes` landing confirmation,
the claim stays held. Prepare only the difference at
[the `/next-plan` steps](../SKILL.md#steps) step 4, whose brief cites
the existing preparation snapshot, with its `## In scope` and `## Out of scope`,
in place of the deleted claimed Plan; run step 5, then step 8 on top of the
unlanded candidate commit, because the request is a choice under
[`### Implementation approval`](../SKILL.md#implementation-approval), which
decides whether step 7 presents; skip steps
6, 9, and 10, which already ran, and skip the checkpoint for every later outcome
of the run; then dispatch a fresh `/finalize-changes` preparation with the
step-10 result, because the claim exit already ran.
