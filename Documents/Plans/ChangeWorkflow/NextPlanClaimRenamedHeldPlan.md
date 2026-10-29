<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-29T14:56:29.791Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — re-claim of a renamed held Plan reports it absent

## Context
Observed symptom. A `/next-plan` run held a claim on a Plan at its old path
under `Documents/Plans/Engine/` and, at step 6's final claim refresh, ran
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan '<old path>'`.
The primary branch had moved from aa47e4a3 to f91254a9 under the session, and
f91254a9 renamed the held Plan from `Documents/Plans/Engine/` to
`Documents/Plans/Game/` (`git show -M --name-status f91254a9`); that run later
completed and deleted the Plan.

The script fast-forwarded the session, its `plan validate` run healed the held
claim away (the old path is no longer present at the primary tip; the result
projects `healedClaims`, `Invoke-NextPlanClaim.ps1:109-110`),
`plan claim-next --plan <old path>` returned `none`, and
`Get-TargetedNoneMessage` fell through to its last branch
(`Invoke-NextPlanClaim.ps1:38`). The result was `status: pass`,
`code: none-available`, message "The requested Plan '<old path>' is absent from
the session tree.", `nextAction: checkpoint-followup-gate`. Nothing in the
result named the new path or said a rename had happened;
`.agents/skills/next-plan/references/claim-results.md:112-119` documents only
the plain absent case. Workaround: main read `git log` and `git show` over the
range to find the move, asked the user, and re-claimed at the new path.

Second symptom on the same recovery path. That re-claim at the new path
returned `claim.worktree-dirty` (`Invoke-NextPlanClaim.ps1:79`) because
`Documents/Plans/Engine/CollisionFlightPlaneLayers.md`, a follow-up Plan the
same run's `/create-follow-up-plans` dispatch had written and staged, was
uncommitted scheduler input. The gate exempts a run only while it still holds a
claim (`Invoke-NextPlanClaim.ps1:72-78`), and the healing above had removed it.
Workaround, the one the message names: `git stash push -u -m <tag> -- <that
Plan>`, re-claim, `git stash apply --index <entry sha>`, then drop the entry —
on the stash stack every worktree and session shares.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: be3ffb28-d3d7-4d8b-83f2-81a23f5d1939
- Worktree/branch UUID: 72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Session branch: claude/72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Worktree: .claude\worktrees\BrokenEngine\72ca68d0-0831-42bf-a368-d8e2ca9ab1ae
- Landing ref: claude/72ca68d0-0831-42bf-a368-d8e2ca9ab1ae, the session branch
  above, whose tip is that session's final commit and which survives exactly as
  long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode — the landing ref
— supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Choices left to the fix session, with the author's recommendations:
- Report or follow the rename. When a refresh's fast-forward moved the
  requested or held Plan, the script can name the new path in its result and
  stop for the user, or claim the new path itself. The author recommends
  naming the new path and stopping, because the moved Plan's content may have
  changed under the preparation evidence; the user then confirms the re-claim.
- The second symptom. The author recommends fixing the first symptom first and
  then re-checking whether the dirty-scheduler gate is still reached on this
  path, since that gate is reached only after the held claim is lost; change
  the gate only if it still forces the stash workaround.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above:
  `Get-TargetedNoneMessage` and the none branch of the claim loop, the
  dirty-worktree gate, and the result `schemaVersion` in
  `Invoke-NextPlanClaim.ps1`; the absent-result wording in `claim-results.md`
  `## Reading the queue listing`; and, only if the result shape or
  `nextAction` changes, the step 6 final claim refresh paragraph of
  `/next-plan` `SKILL.md`

## Out of scope
- The landed change the session produced
- WorktreeCli's claim healing and `plan claim-next`; a fix that needs them
  changed is surfaced for re-planning under `Documents/Plans/Tools/`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one script's claim result); escalate if
the fix reaches build/bootstrap coordination. A changed result shape bumps the
result `schemaVersion`. A run never claims a Plan the user did not confirm.
Never embed transcript paths or home paths.

## Acceptance criteria
- A claim refresh whose fast-forward moves the held or requested Plan to a new
  path reports that new path (or claims it, per the chosen design) instead of
  saying the Plan is absent from the session tree
- In the recorded sequence (held claim, staged follow-up Plan under
  `Documents/Plans/`, fast-forward that moves the held Plan), the run reaches a
  held claim on the new path without setting the follow-up Plan aside on the
  stash stack
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Both symptoms are one Plan: they share one script and one recovery path, and
the second is reachable only after the first loses the held claim.
