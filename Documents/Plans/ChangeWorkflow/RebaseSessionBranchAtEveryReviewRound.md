<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T03:02:07.486Z","dependsOn":[]} -->
# Rebase the session branch at every review round

## Context

The user directed this Plan: "At every review loop, the branch should be
rebased from main and any merge conflicts resolved at that point (so we don't
get to the end and discover 17 commiets to rebase)". A review loop here is a
review round as the `.agents/scripts/Get-ChangeWorkflowSteps.ps1` header
defines it (round, snapshot, since point `S_k`, round diff `D(S_k)`, and the
landing check `Test-ReviewRoundCoverage` in
`.agents/scripts/SessionChangeInventory.psm1`).

What happened: the session that built the review rounds ran eight rounds
without rebasing. At landing, primary had advanced 17 commits, and the rebase
conflicted in seven files, among them `Get-ChangeWorkflowSteps.ps1` and
`SessionChangeInventory.psm1`, because primary had changed files the session
rewrote; one primary commit had carried out and deleted a Plan the session was
narrowing. The conflict resolution then needed its own review round.

Current mechanisms:

- Nothing moves the session branch between the session's start and landing.
  The session baseline changes only when an authorized branch synchronization
  reports a new one (`.agents/references/subagent-reporting.md`
  `## Task brief`, the session-baseline paragraph). The only such
  synchronization is the `/next-plan` claim, which fast-forwards only and
  stops with `claim.session-diverged` when the session holds a commit primary
  lacks (`.agents/skills/next-plan/references/claim-results.md`
  `## Session baseline and the sync object`); it also fast-forwards the
  session Coretide clone to its upstream.
- At landing, a rebase that does not reproduce the approved patch is refused
  first, as `rebase.patch-not-identical`
  (`.agents/scripts/FinalizeWorkflowCommon.psm1`, the patch-identity check
  after `git rebase --onto`). A patch-identical rebase reaches the landing
  check, which refuses it with an `inventory.review-round-*` code only when
  the rebased candidate owes a review round; a patch-identical rebase over an
  unrelated primary edit passes. On that refusal main reruns the step script so
  a round covers the rebased bytes (`.agents/skills/finalize-changes/SKILL.md`
  `### Landing confirmation`). So the review of rebase results already exists,
  but only once, at the end.
- `-StartRound` freezes a snapshot of HEAD's tree plus every path `git status`
  reports, and records `Baseline: <session baseline>` in its message (header,
  snapshot form; written by `Format-ReviewRoundSnapshotMessage` in
  `.agents/scripts/ReviewRounds.psm1`). The round reader takes the last
  round's `Baseline` when none is passed (`Get-ReviewRoundState`), and a since
  point's recorded `Baseline` decides which paths were session paths at that
  point (`Get-InventorySideEntry`, the `$Since.Baseline` branch).
- The session's changes stay uncommitted in the working tree until
  finalize preparation builds the candidate commit
  (`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1`).
  `git rebase` refuses a dirty working tree, and `--autostash` stores the
  changes in the stash list, which every worktree of the repository shares,
  when reapplying them conflicts.
- The Coretide clone is its own repository with its own upstream and its own
  review-round chain; each snapshot records `Coretide-Baseline: <its merge
  base with upstream>` (header).
- No file may be edited while a round is open
  (`.agents/references/change-workflow.md` `#### Step 6 — Review and resolve
  correctness`).

## Unresolved decisions

The recommendations below are the author's; the executing session presents
them to the user at plan approval.

1. Where the rebase runs:
   - (a) A step-script row printed before the `-StartRound` row, run by a
     dispatched `implementer`, plus a `-StartRound` refusal (exit 2) when the
     session branch does not contain the primary tip (recommended). The
     refusal makes a skipped rebase impossible, and the row keeps conflict
     resolution, which needs judgment, in an agent dispatch.
   - (b) `-StartRound` itself rebases and refuses on a conflict, leaving the
     resolution to a dispatch the refusal names. Trade-off: one command, but
     the script that freezes rounds also rewrites the branch.
   - (c) A prose step in `change-workflow.md` only. Trade-off: no script
     change, but nothing enforces it.
2. How the uncommitted working tree moves with the rebase:
   - (i) Commit the session changes to the session branch first, then rebase
     (recommended): no shared stash list, and conflicts surface as ordinary
     rebase conflicts. Constraint: finalize preparation and the `/next-plan`
     claim must still accept a session branch holding such commits.
   - (ii) `git rebase --autostash`. Trade-off: no extra commit, but a conflict
     on reapplying lands in the shared stash list.
   - (iii) Move HEAD to the primary tip and three-way merge the working tree.
     Trade-off: no commit and no stash, but a conflict leaves a merge state
     Git tooling handles less uniformly than a rebase.

## Design

1. Before the user answers, establish on a scratch repository built from the
   real scripts how the current since-point rules treat a round frozen after a
   baseline move: a closed round, then primary changes a session path and a
   non-session path, then a rebase and `-StartRound` with the new baseline.
   Record which hunks each owed review reads, whether primary's own hunks in a
   session path are owed review, and whether a conflict-resolution hunk is owed
   review. Change no tracked file in this step. A ready scratch setup is at
   commit `94fa961365e8cc97584299d4fc5b81266e5ee847` on primary, which holds
   `Documents/Plans/ChangeWorkflow/KeepReviewRoundScenarioScripts/lib.ps1` and
   `check13.ps1` (the scratch-repository, Coretide-clone, and closed-round
   setup) whether or not that Plan's support folder still exists: write both
   files with `git show <commit>:<path>` into one new directory under `Temp/`
   and run, from the worktree root,
   `pwsh -NoProfile -NonInteractive -File Temp/<directory>/check13.ps1 -Scripts . -Tag <new tag>`.
2. After the answer, carry out the chosen options:
   - The rebase brings the session branch onto the primary tip and, when the
     clone exists, the Coretide clone onto its upstream, Coretide first, as
     the freeze orders them. It runs only while no round is open.
   - Its result names the new session baseline, which main records under the
     session-baseline rule and passes as `-Baseline` to the next
     `-StartRound`, so that snapshot's `Baseline` line holds it.
   - A conflict is resolved under the reconciliation-conflict bullet of
     `.agents/skills/finalize-changes/references/worker.md` `## Recovery`
     (hunk by hunk, keeping behavior one side already had), and the round
     that follows reviews the resolution. If step 1 shows the since-point
     rules would not owe that review, the smallest rule change that makes
     them owe it is in scope; any wider change returns for re-planning.
   - The session-baseline paragraph of `subagent-reporting.md` names the new
     synchronization; the step-script header and `change-workflow.md`
     Step 6 state where the rebase runs.

Change Workflow tier: Tier 3 — the change moves the session baseline mid-change
and changes what the landing gate's review coverage reads (a trust boundary of
the landing gate), across the step script, the Change Workflow reference, and
the Coretide clone handling. No C++, shader, build, `.pack`, replay, or
live-verification exposure.

## Critical files

- `.agents/scripts/Get-ChangeWorkflowSteps.ps1` (header, default-run rows,
  `-StartRound`)
- `.agents/scripts/SessionChangeInventory.psm1` (`Get-ReviewRoundState`, only
  as Design 2 allows)
- `.agents/references/change-workflow.md` (`#### Step 6 — Review and resolve
  correctness`)
- `.agents/references/subagent-reporting.md` (`## Task brief`,
  session-baseline paragraph)

## In scope

- Before the user answers: Design 1 on scratch repositories under `Temp/`,
  with no tracked change.
- After the answer: the rebase under the chosen options of both decisions,
  in the step script (and, under decision 1 (a), one new script it names if
  the row needs one); the `-StartRound` refusal under decision 1 (a); the
  since-point rule change Design 2 allows; and the prose in the critical
  files naming the rebase and the baseline move.

## Out of scope

- The finalizer's own landing rebase and recovery routing, owned by
  `Documents/Plans/ChangeWorkflow/StateReviewRoundRequirementInLandingResult.md`.
- The review-round chain's lifetime across tasks, owned by
  `Documents/Plans/ChangeWorkflow/EndReviewRoundChainWithItsTask.md`.
- The `/next-plan` claim's fast-forward, except where decision 2 (i) needs it
  to accept a session branch holding session commits.
- The round message formats, other than the value the `Baseline` line holds.

## Acceptance criteria

- The executing session records the user's answers to both decisions and the
  Design 1 findings.
- In a scratch scenario where primary advances during a session and changes a
  file the session also changed, the next round's `-StartRound` freezes a
  snapshot whose tree contains the primary tip's changes and whose
  `Baseline` line names the new baseline; the conflict-resolution hunk is in a
  review the round owes; and the landing rebase afterwards has no commit to
  replay that the last round did not see.
- The same holds for the Coretide clone against its upstream.
- Under decision 1 (a), `-StartRound` refuses while the session branch lacks
  the primary tip.
- The static-checks runner passes for the changed files.
