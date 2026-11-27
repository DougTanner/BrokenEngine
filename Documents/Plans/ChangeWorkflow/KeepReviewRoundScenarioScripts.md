<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T01:04:39.841Z","dependsOn":[]} -->
# Decide whether to keep the review-round scenario scripts permanently

## Context

The Change Workflow review rounds (`.agents/scripts/Get-ChangeWorkflowSteps.ps1`
header: round, snapshot, since point, and the landing check
`Test-ReviewRoundCoverage` in `.agents/scripts/SessionChangeInventory.psm1`),
the sweep round (`.agents/skills/sweep/scripts/Invoke-Sweep.ps1 -ReviewRound`),
and the `-Landing` inventory were built and reviewed over six review rounds.
Each round's fixes were checked with deterministic PowerShell scenario
scripts kept only as gitignored scratch files. They are copied unchanged into
this Plan's support folder `KeepReviewRoundScenarioScripts/`. Each builds
scratch Git repositories, runs the step script with `-StartRound` and
`-CloseRound` with fixed results, `Invoke-Sweep.ps1 -ReviewRound`, and the
`-Landing` inventory, and prints one `PASS:` or `FAIL:` line per assertion on
the printed rows and results. No agent or model runs.

They grew one review round at a time and overlap:

- `lib.ps1` — shared helpers (`New-ScratchRepo`, `Invoke-Steps`, `Assert`,
  `Get-RoundHead`, `Get-RefTip`), dot-sourced by every check.
- `check2.ps1`-`check5.ps1` — the first acceptance checks (non-landing
  rounds, loop rules with `-Landing`, the landing check, Coretide snapshots
  and the split `/save-plan`), chained in one scratch repository.
  `run-all.ps1` runs them against the worktree's scripts; `run-all-p.ps1`
  runs copies of them against another tree.
- `check7.ps1`, `check7p.ps1`, `check7r.ps1` — the sweep round; `check7p.ps1`
  is `check7.ps1` with `-Scripts`/`-Tag`, and `check7r.ps1` adds the refused
  sweep recovery case.
- `check8.ps1`-`check13.ps1` — one reproducing case per accepted finding of
  each review round; `check11-edges.ps1` prints verdicts and asserts nothing.

How they run (exact runs in Design 1): scratch repositories go under the
script's own directory, and `check4.ps1`, `check5.ps1`, `check7.ps1`,
`check7p.ps1`, and `check7r.ps1` write landing JSON under `Temp/loop-scenario/`
relative to the working directory. The parameterized scripts take
`-Scripts <tree>` (the directory holding the `.agents` tree under test, `.` for
the worktree) and `-Tag <new tag>` (a fresh scratch directory name);
`check2.ps1`-`check5.ps1`, `check7.ps1`, and `run-all.ps1` take no parameters
and use fixed scratch directories that must not exist yet. Some header comments
cite scratch files of the authoring session; they are history only.

Known stale assertions: after the authoring session undid its sweep recovery
attempt, two `check7r.ps1` assertions fail by design — "VS014: -ReviewRound
freezes a second sweep round on the recovered branch" and "VS014: the landing
inventory passes after the second sweep round". They are the reproducing case
of `Documents/Plans/ChangeWorkflow/RecoverSweepAfterLandingRefusesItsCandidate.md`.
Every other script passed in the authoring session's last recorded run. The
copied `check13.ps1` holds that run's final Tier-3 case (the `COH602/ADV030`
section: a closed Tier-3 chain with the reviewed file restored to baseline).
Script changes after that run are not recorded here, so Design 1's run settles
the current result.

The user directed this Plan: "Add a followup plan to consider adding them
permanently, the unit test prohibition is mostly for the game and engine
code". Root `AGENTS.md` `## Directives` states "Do not add unit tests" without
that qualification.

An open question such a suite could settle is convergence: in the third review
round, six of the seven Critical or High defects had been introduced or left
open by the previous round's fix. The hand-written cases cover only the
sequences reviewers found; whether the rules hold for every sequence was judged
answerable only by a generated sweep over (tier, landing mode, per-key results,
diff kind, rebase) sequences.

The review-round chain's lifetime across tasks in one worktree is owned by
`Documents/Plans/ChangeWorkflow/EndReviewRoundChainWithItsTask.md`, not by this
Plan.

## Unresolved decisions

The recommendations below are the author's; the executing session presents
both decisions to the user at plan approval.

1. Whether to keep the scenarios:
   - (a) Keep one consolidated suite (recommended): one directory such as
     `.agents/scripts/ReviewRoundScenarios/` holding `lib.ps1`, one case file
     per concern (rounds, loop rules, landing check, Coretide and split save,
     sweep round), and one runner taking `-Scripts` and `-Tag`, writing only
     under `Temp/`. Overlapping cases are merged, stale assertions dropped, and
     `check7.ps1`, `run-all.ps1`, and `check11-edges.ps1` dropped, since other
     files supersede them or they assert nothing. Trade-off: the most work
     now, but one entry point to rerun after each change; the rules have
     repeatedly regressed under per-finding fixes, and the cases are cheap to
     rerun.
   - (b) Keep them as copied in one directory such as
     `.agents/scripts/ReviewRoundScenarios/`, with only stale assertions
     removed. They keep their separate entry points and are run as Design 1
     runs them, from a copy under `Temp/`, because they create scratch
     repositories beside themselves. Trade-off: little work now, but
     overlapping files, several entry points, a copy step before each run, and
     cases that duplicate each other must each be maintained.
   - (c) Do not keep them; the support folder is removed with the Plan.
     Trade-off: no maintenance and no conflict with the root `AGENTS.md`
     directive, but each later change to the review rounds is checked only
     by review.

   Under (a) or (b), the root `AGENTS.md` "Do not add unit tests" directive is
   rewritten to the scope the user stated (game and engine code), so the suite
   does not contradict it, and the way to run the suite (under (a) its runner,
   under (b) the copy-and-run procedure of Design 1) is named once where the
   scripts it covers are documented (the `Get-ChangeWorkflowSteps.ps1`
   header).
2. Whether a generated sequence sweep should later replace the hand-written
   cases: (i) file it as its own follow-up Plan, or (ii) do not pursue it.
   Recommended: decide it here but do not build it in this Plan; under (i) it
   is a separate stage with its own design. Trade-off: (i) can settle the
   convergence question but is a larger design; (ii) relies on the
   hand-written cases alone.

## Design

1. Before the user answers, run the copied scripts and classify each failure.
   Copy the support folder's files to `Temp/loop-scenario/` under the session
   worktree root, then run from the worktree root, giving each run a `-Tag`
   that names no existing directory under `Temp/loop-scenario/`:
   - `lib.ps1` is a helper; do not run it.
   - `check2.ps1`-`check5.ps1`: run them through their one entry point,
     `pwsh -NoProfile -NonInteractive -File Temp/loop-scenario/run-all-p.ps1 -Scripts . -Tag <tag>`,
     which copies them with `lib.ps1` into `Temp/loop-scenario/ra-<tag>/` and
     runs them there in order with fresh scratch repositories. Do not run
     `run-all.ps1` or those four checks directly.
   - `check7.ps1`: do not run it; `check7p.ps1` is the same check with
     parameters.
   - `check7p.ps1`, `check7r.ps1`, `check8.ps1`-`check13.ps1`, and
     `check11-edges.ps1`: run each as
     `pwsh -NoProfile -NonInteractive -File Temp/loop-scenario/<script> -Scripts . -Tag <tag>`
     (`check11.ps1` keeps its optional `-Docs` default, the worktree root).
     `check11-edges.ps1` prints verdicts and asserts nothing; record them.

   List each `FAIL:` line. The two `check7r.ps1` assertions named in
   `## Context` are stale. Any other failure whose assertion contradicts the
   landed `Get-ChangeWorkflowSteps.ps1` header or the landed landing check is
   stale; every other failure is a defect for a follow-up Plan, never fixed
   here.
2. After the user answers, carry out the chosen option of decision 1 and
   record decision 2, filing its follow-up Plan under (i).

Change Workflow tier: Tier 2 — scoped tool behavior: new scenario scripts and
their runner for one subsystem's scripts, plus a root `AGENTS.md` wording
change. No determinism, wire, serialization, threading, or trust-boundary
exposure; no C++, shader, build, `.pack`, replay, or live-verification
exposure.

## Critical files

- `Documents/Plans/ChangeWorkflow/KeepReviewRoundScenarioScripts/` (support
  folder, all files)
- `AGENTS.md` (`## Directives`, the "Do not add unit tests" bullet)
- `.agents/scripts/Get-ChangeWorkflowSteps.ps1` (header)

## In scope

- Before the user answers: running the copied scripts under `Temp/` and
  classifying each failure (Design 1), with no tracked change.
- After the answer, under any option: one follow-up Plan per real failure from
  Design 1, and, under decision 2 (i), the generated-sweep follow-up Plan.
- Under option (a): the new suite directory, its files, and its runner.
- Under option (b): the suite directory holding the copied files with only
  stale assertions removed.
- Under option (a) or (b): the root `AGENTS.md` "Do not add unit tests"
  bullet, and one header sentence in `Get-ChangeWorkflowSteps.ps1` naming how
  the suite runs.
- Under option (c): the support folder's removal.

## Out of scope

- Any behavior change in the scripts under test: `Get-ChangeWorkflowSteps.ps1`,
  `ReviewRounds.psm1`, `SessionChangeInventory.psm1`, `Invoke-Sweep.ps1`, and
  the finalizer scripts. A real failure becomes a follow-up Plan.
- Building the generated sequence sweep (decision 2).
- The review-round chain's lifetime, owned by
  `Documents/Plans/ChangeWorkflow/EndReviewRoundChainWithItsTask.md`.
- Tests of game, engine, or other C++ code.
- Running the suite from the static-checks runner, a hook, or a landing gate.

## Acceptance criteria

- The executing session records the user's answers to both unresolved
  decisions.
- Under (a): the runner, run from the worktree root with `-Scripts .` and a
  new `-Tag`, prints no `FAIL:` line and writes nothing outside `Temp/`.
- Under (b): each entry point Design 1 runs (`run-all-p.ps1`, `check7p.ps1`,
  `check7r.ps1`, `check8.ps1`-`check13.ps1`, and `check11-edges.ps1`), copied
  to `Temp/` and run as Design 1 runs it, prints no `FAIL:` line and writes
  nothing outside `Temp/`.
- Under (a) or (b): root `AGENTS.md` no longer forbids the kept suite.
- Under (c): no tracked change other than the support folder's removal and
  the follow-up Plans In scope names.
- Every real failure found in Design 1 is filed as a follow-up Plan.
- The static-checks runner passes for the changed files.

## Notes

- `Documents/Plans/ChangeWorkflow/RecoverSweepAfterLandingRefusesItsCandidate.md`
  carries its own copy of `check7r.ps1` and `lib.ps1`; the two Plans are
  independent.
