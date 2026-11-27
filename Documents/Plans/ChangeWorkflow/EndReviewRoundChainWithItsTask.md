<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T01:36:53.584Z","dependsOn":[]} -->
# End the review-round chain with its task

## Context

The review rounds of each repository form one chain under
`refs/worktree/review-rounds`: one in the engine worktree and one in the
session Coretide clone (`.agents/scripts/Get-ChangeWorkflowSteps.ps1` header).
The header states "One chain per repository spans every round the worktree
runs", and nothing ends a chain. The only writers of either ref are the
compare-and-swap advances through `Update-ReviewRoundRef` in
`Get-ChangeWorkflowSteps.ps1` (`-StartRound`, `-CloseRound`) and
`.agents/skills/sweep/scripts/Invoke-Sweep.ps1 -ReviewRound`. No script under
`.agents/skills/finalize-changes/scripts/` names the ref, nor does
`.agents/skills/reset-worktree/scripts/Reset-AgentWorktree.ps1`.

So a task that starts in a worktree after an earlier task landed there, or
after `/reset-worktree` reused the worktree, inherits the earlier task's rounds
and since points. A key's since point is the snapshot of the last round whose
close gave that key a result below High (the header's `S_k`), and the round
diff runs from it to the new snapshot. An edit that restores a file to an old
snapshot's bytes therefore looks already reviewed.

Observed by review on a scratch fixture with the real scripts and a real closed
round: round 1 reviewed line 3 of a document and closed. A later task in the
same worktree, on a baseline where primary had edited line 9 of that document,
undid that line-9 edit, so the candidate held round 1's snapshot bytes. The
session diff triggered `/coherence-review`, yet the step script reported the
review rounds done and the landing check (`Test-ReviewRoundCoverage` in
`.agents/scripts/SessionChangeInventory.psm1`) passed. An earlier fix of the
same cause keeps the session row of a path that was not a session path at the
since point (`SessionChangeInventory.psm1`, the comment "A session path that
was not one at the since commit is new to review"); here the document already
was a session path at the old since point, so that fix does not apply.

Root cause: the chain outlives its task. The header's reading rule treats an
absent chain as no reviewed round, so a task that starts with no chain owes
its first round from the session baseline.

## Design

Recommended: end both repositories' chains when their task ends, by deleting
`refs/worktree/review-rounds` in the engine worktree and in the Coretide clone
(when the clone exists):

- On landing completion. `.agents/skills/finalize-changes/references/worker.md`
  `## Steps` step 9 lands a stage's Coretide commit before its engine landing,
  and a blocked engine landing goes through recovery, which reruns the landing
  check (`Get-ReviewRoundCoverageFailure`) over both chains. Constraint: neither
  chain ends until the engine landing has succeeded, so a stage that lands its
  Coretide commit and then its engine commit, including an engine landing
  recovered after the Coretide side landed, keeps passing the landing check. A
  Coretide-only landing ends both chains once its `-Land` succeeds.
- On worktree reset: `Reset-AgentWorktree.ps1` ends both chains.

The executing session places each deletion in the scripts `## In scope` names
and chooses how a failed deletion is reported, recommended as a landing
residual the way a failed claim deletion is (worker step 13), because primary
has already advanced. The header sentence above is rewritten to say a chain
spans one task's rounds and where it ends.

Change Workflow tier: Tier 3 — changes which reviews the landing gate counts
as done across tasks (a trust boundary of the landing gate), across the
finalize-changes and reset-worktree scripts, which are independently owned. No
C++, shader, build, `.pack`, replay, or live-verification exposure.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1`
- `.agents/skills/reset-worktree/scripts/Reset-AgentWorktree.ps1`
- `.agents/scripts/Get-ChangeWorkflowSteps.ps1` (header)

## In scope

- `Invoke-FinalizeLanding.ps1`: ending both chains after a successful engine
  landing.
- `Invoke-FinalizeCoretide.ps1 -Land`: ending both chains after a successful
  Coretide-only landing.
- `Reset-AgentWorktree.ps1`: ending both chains.
- The `Get-ChangeWorkflowSteps.ps1` header sentence on the chain's span.
- The reference text that documents those scripts' results, only where a new
  result field or residual is added.

## Out of scope

- The since-point rules, the chain reading rule, the round message formats,
  and the landing check's rules in `SessionChangeInventory.psm1`.
- Since points within one task.
- Sweep recovery after a refused landing, owned by
  `Documents/Plans/ChangeWorkflow/RecoverSweepAfterLandingRefusesItsCandidate.md`.
- The scenario suite, owned by
  `Documents/Plans/ChangeWorkflow/KeepReviewRoundScenarioScripts.md`.

## Acceptance criteria

- In a scratch scenario reproducing `## Context` (a closed round reviewing
  line 3, a landing, a new task on a baseline where primary edited line 9, that
  edit undone), the step script prints a first round that owes
  `/coherence-review`, and the landing check refuses until that round closes.
- After a successful engine landing, a Coretide-only landing, or a
  `Reset-AgentWorktree.ps1` run, `refs/worktree/review-rounds` resolves in
  neither the engine worktree nor the Coretide clone.
- A stage that lands its Coretide commit and then its engine commit passes
  the engine landing check, including an engine landing recovered after the
  Coretide side landed.
- The static-checks runner passes for the changed files.

## Notes

- `Documents/Plans/ChangeWorkflow/KeepReviewRoundScenarioScripts/lib.ps1` and
  `check13.ps1` hold scratch-repository, Coretide-clone, and closed-round setup
  to start from.
