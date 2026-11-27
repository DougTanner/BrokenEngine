<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T01:04:42.340Z","dependsOn":[]} -->
# Recover a sweep whose landing refuses its rebased candidate

## Context

A `/sweep` run skips the Change Workflow. After its stage close it freezes one
closed sweep round with
`.agents/skills/sweep/scripts/Invoke-Sweep.ps1 -Spec <spec> -ReviewRound`,
which requires HEAD to equal the sweep baseline recorded in
`Temp/Sweep/Baseline.txt` (`Read-SweepBaseline` throws "belongs to commit"
otherwise). A sweep round counts for every review key's since point
(`.agents/scripts/Get-ChangeWorkflowSteps.ps1` header).

When primary edits a swept file before the sweep lands, the finalizer's clean
rebase changes the candidate's bytes. The landing check
(`Test-ReviewRoundCoverage` in `.agents/scripts/SessionChangeInventory.psm1`)
then owes `/coherence-review` from the sweep snapshot to the rebased candidate
and refuses with `inventory.review-round-unfinished`;
`Invoke-FinalizeLanding.ps1` `Assert-LandingReviewRoundCoverage` restores the
branch to the approved commit and returns disposition `recovery`. No round
can cover the rebased bytes: the sweep has no further round, and
`-ReviewRound` refuses the recovered HEAD. Today main reports that refusal to
the user as a blocker (`.agents/skills/sweep/SKILL.md` step 11 and
`.agents/skills/finalize-changes`).

Reproducing case: the support folder's `check7r.ps1` (with `lib.ps1`), block
"VS014". It sweeps one paragraph of `doc.md`, freezes the sweep round, commits
on branch `session`, adds a primary commit editing another paragraph of
`doc.md`, rebases `session`, and runs the `-Landing` inventory. Run it from the
worktree root on a copy in `Temp/loop-scenario/`:
`pwsh -NoProfile -NonInteractive -File Temp/loop-scenario/check7r.ps1 -Scripts . -Tag <new tag>`.
Its precondition assertion passes today, and its two "VS014:" assertions fail.

An earlier attempt let `-ReviewRound` accept a HEAD that descends from the
sweep baseline and reran sweep steps 9-11 after the refusal. It was undone
after review found three defects, which a recovery design must avoid:

- The re-frozen round recorded the original sweep baseline, so a primary edit
  inherited by the rebase counted as swept: after `/reset-worktree`, a later
  session restoring that file to the snapshot's bytes got no review, and the
  landing check agreed.
- A refusal from finalize preparation (not from landing) carries no recovery
  disposition, so main re-froze the round without the static checks and builds
  the rebased tree needed.
- A sweep begun on an unlanded session commit, which finalize preparation later
  squashes away, could not re-freeze at all.

## Design

The design choices are the author's recommendations, pre-staged for the
executing session's `/plan-alternatives` and plan review:

1. Recommended shape: a recovered sweep re-freezes one more sweep round on the
   recovered branch. Its snapshot keeps the original sweep baseline only for
   specification ownership and ancestry, and records the recovered session's
   merge base as the round's `Baseline`, so review coverage never credits
   inherited primary edits.
2. Before the re-freeze, the recovered tree passes the static checks and the
   builds the sweep's own close required, whether the refusal came from
   landing or from preparation.
3. A sweep whose baseline is not an ancestor of the recovered HEAD stays a
   user blocker, reported as today.
4. Update the one owning sentence that now reports the refusal as a blocker
   (sweep step 11 and its finalize-changes references) to the new procedure.

Alternative to weigh: on refusal, route the rebased candidate through a normal
Change Workflow review round instead of a second sweep round, which needs no
sweep-specific baseline rule but makes a sweep depend on the workflow it skips.

Change Workflow tier: Tier 3 — spans independently owned subsystems (`/sweep`,
`/finalize-changes`, and the review-round landing check) and changes what the
landing check trusts as reviewed. No C++, shader, build, `.pack`, replay, or
live-verification exposure.

## Critical files

- `.agents/skills/sweep/scripts/Invoke-Sweep.ps1`
- `.agents/skills/sweep/SKILL.md`
- `.agents/skills/finalize-changes/SKILL.md`
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md`
- `.agents/skills/finalize-changes/references/worker.md`
- `.agents/scripts/Get-ChangeWorkflowSteps.ps1` (header: sweep round form)

## In scope

- `Invoke-Sweep.ps1`: the header, `Read-SweepBaseline`, and the
  `-ReviewRound` block.
- The sweep round's form in the `Get-ChangeWorkflowSteps.ps1` header and its
  reader in `SessionChangeInventory.psm1` `ConvertFrom-ReviewRoundMessage` and
  `Read-ReviewRoundChain`, only if the recorded `Baseline` rule changes.
- `sweep/SKILL.md` step 11 and the finalize-changes sentences that route an
  `inventory.review-round-*` refusal of a sweep.

## Out of scope

- The review-round rules for ordinary Change Workflow rounds.
- `Invoke-FinalizeLanding.ps1` and `Invoke-FinalizeCoretide.ps1` result
  shapes, owned by
  `Documents/Plans/ChangeWorkflow/StateReviewRoundRequirementInLandingResult.md`.
- Sweep batches, close, and the Codex dispatch.

## Acceptance criteria

- `check7r.ps1` "VS014" block, run against `-Scripts .`, passes: after the
  recovery the landing inventory passes for the rebased candidate.
- A new case shows that the re-frozen round does not count an inherited
  primary edit to another file as swept: that file, restored later to the
  snapshot's bytes in a new session, still owes `/coherence-review`.
- A new case shows a preparation refusal runs the static checks before the
  re-freeze.
- `check7r.ps1` controls still pass: `-Batch` and `-Close` refuse a HEAD other
  than the sweep baseline, and `-ReviewRound` refuses a HEAD that does not
  descend from it.
- The static-checks runner passes for the changed files.

## Notes

- `Documents/Plans/ChangeWorkflow/KeepReviewRoundScenarioScripts.md` carries
  the same scripts in its own support folder; the two Plans are independent.
