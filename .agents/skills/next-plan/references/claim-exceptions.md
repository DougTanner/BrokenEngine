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
([claimed-plan.md](../../prepare-change/references/claimed-plan.md)
`## Stage split`) is not an approval: it is that explicit current-session user
instruction, and the run takes this deferral route instead of implementation.

## Pixel-evidence deferral

When preparation or a harness run shows that a runtime acceptance criterion of
the claimed Plan can be settled only by reading values off rendered pixels,
because no harness query exposes them, the pixel-evidence prohibition in
`/agent-harness` `## Handoff` binds main too: never settle it from a screenshot
yourself. Report the gap and, on the explicit user deferral instruction
[`../SKILL.md`](../SKILL.md) `## Rules` requires, dispatch
`/create-follow-up-plans` (citing the lacking command doc, client-computed
values, and narrowest query) to file the missing query and add its path to the
claimed Plan's `dependsOn`: for a new Plan, that worker adds its chosen path
before `New-PlanFile.ps1`
([`new-plan-file.md`](../../../references/new-plan-file.md#invocation)); for a
reused Plan, update `dependsOn` (dependency-change case) and validate — on
`dependency-cycle`, stop without deferring and ask the user rather than drop an
existing prerequisite. Defer only after creation or reuse and validation
succeed. That prerequisite and edit are already in the worktree at the
checkpoint and feed the followup-only landing gate under the deferred row in
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
`nextAction`: report the result's `holder`
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
[the `/next-plan` steps](../SKILL.md#steps) step 4 using the difference re-entry
brief in [`prepare-change/SKILL.md`](../../prepare-change/SKILL.md) `## Inputs`
(`mode: claimed-plan`); run step 5, then step 8 on top of the unlanded candidate
commit, because the request is a choice under
[`### Implementation approval`](../SKILL.md#implementation-approval), which
decides whether step 7 presents; skip steps 6, 9, and 10, which already ran, and
skip the checkpoint for every later outcome of the run; then dispatch a fresh
`implementer` for the `/finalize-changes` preparation with the step-10 result,
because the claim exit already ran.
