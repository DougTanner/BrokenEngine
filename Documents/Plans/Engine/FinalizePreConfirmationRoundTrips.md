<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T01:24:03.309Z","dependsOn":[]} -->
# Remove the finalizer's decision-free manager round trips

## Context

Landing commit `94d2bb9d` took roughly 660 seconds of finalizer wall clock
before the SmartGit approval review could open, across four `implementer`
dispatches: 147 s ending BLOCKED, 404 s (rebase, preparation, acceptance table,
movement check), 114 s (rescoring the same table from pasted handoffs), and 88 s
landing. Two of the four runs produced no decision content. The user named the
delay "pointless ceremony" and directed: "A simple no-conflict rebase should
happen 100% deterministically inside a script. File a followup plan to
investigate and fix."

Two distinct round trips are recorded.

**1. Stale base.** `Invoke-FinalizeApprovalPreparation.ps1:340-346` blocks with
`git.primary-not-ancestor` ("Session tip does not contain the current primary
tip; rebase the session onto primary before approval preparation.") whenever
primary advanced under the session. `references/worker.md:215-222` (`## Rules`)
and `:258-263` (`## Recovery`) then require the finalizer to "stop and report
the result to the manager. After the manager classifies it, if recovery is
authorized, this same finalizer performs the documented ordinary linear `git
rebase` and re-invokes approval preparation."
`references/scripts.md:142-152` owns those mechanics: ordinary linear `git
rebase refs/heads/<primary-branch>`, never `--onto`, then re-invocation with
re-resolved tips. `SKILL.md:44-45` routes it: "A stale-base result loops back to
main before any launch line is returned."

In this session the advancing primary commit `ed11ba01` touched none of the
session's nine paths, the rebase replayed cleanly, and `git patch-id --stable`
was identical before and after. The manager's "classification" was therefore a
message pair that decided nothing, and the finalizer re-ran candidate creation,
lock claim, preparation, and lock release around it.

The post-confirmation path already does this deterministically inside a script:
`Invoke-FinalizeLanding.ps1:660-683` performs at most one bounded internal
rebase, verifies patch identity, and only stops on the real failures
(`rebase.conflicted`, `rebase.patch-not-identical`). `worker.md:117-118` and
`SKILL.md:111-113` state the same policy ("A clean identical rebase onto an
advanced primary lands without re-asking"). So a clean rebase is script work
after the confirmation and a manager decision before it.

**2. Acceptance-table handoffs.** `worker.md:75-84` tells the finalizer to fill
the table "from the hygiene handoffs that already exist", and
`references/landing-acceptance-table.md:49` and `:79-80` correctly forbid
scoring from narrative ("Narrative alone is `UNVERIFIED`"; a missing typed
artifact "is a `BLOCKED` row, never a `PASS`"). But `SKILL.md:24-36`
(`## Inputs`) requires only the objective, stage decisions, and caller-owned
paths, so nothing obliges main to hand the finalizer the review handoffs it must
cite. In this session four rows (the combined `/coherence-review` pass,
`/resolve-findings` plus re-review, the `/validate-skill` semantic half, and
`/progressive-disclosure-review`) scored UNVERIFIED/BLOCKED purely because those
handoffs lived in main's context. Main pasted all four into a resume message and
the finalizer rescored them unchanged — a second full round trip.

**3. Pre-confirmation lock cycles.** `scripts.md:189-193` requires the lease to
be claimed before approval preparation and released before any user wait, and
`worker.md:35-40` repeats that ordering. Because the stale-base block restarts
the sequence, this session claimed and released the landing lock twice before
the confirmation. The lock itself guards only the primary advance
(`scripts.md:230`; root `AGENTS.md:157`), and the movement check is read-only
and takes no lease (`scripts.md:84-85`).

Impact: every landing that races an advancing primary pays two manager round
trips and a duplicated lock cycle that decide nothing, delaying the one thing
the user is waiting on — the SmartGit review.

## Design

Author's recommendation, in three parts. Each is small and independently
verifiable; the third is expected to fall out of the first.

**A. Rebase inside approval preparation.** Give
`Invoke-FinalizeApprovalPreparation.ps1` the clean-rebase case itself. At the
`:340-346` ancestry check, when the session tip does not contain the live
primary tip, run the same ordinary linear `git rebase
refs/heads/<primary-branch>` that `scripts.md:142-152` already prescribes to the
caller, re-resolve both tips from the rebased branch, and continue the existing
squash on those re-resolved values. A rebase that does not apply cleanly aborts,
restores the branch, and still blocks to the manager — the existing conflict
route is unchanged. The result should report what happened (rebase performed or
not needed, the pre- and post-rebase session tips, the primary tip rebased onto)
so the finalizer can state it in the landing summary rather than infer it.

Two interface details the fix session must settle from the code, not assume:

- `-ExpectedCurrentTip` is supplied by the caller and is invalidated by the
  rebase the script itself now performs; decide whether the script re-resolves
  it internally after rebasing or the parameter's meaning becomes "the tip
  before reconciliation". `scripts.md:45-49` and `:150-152` also make an
  earlier-resolved `<baseline>` stale, so the finalizer must not carry a
  pre-rebase baseline past this script.
- The optional `-VerifiedCandidateCommit`/`-VerifiedCandidateTree` gate
  (`scripts.md:157-167`) compares whole trees. After a rebase that replays
  foreign primary bytes the session tree legitimately differs, so that
  comparison must be made against the pre-rebase session tree to keep its
  meaning ("the reconciled session tree still equals a tree that was already
  reviewed") instead of firing `candidate.tree-changed` on a clean rebase.

Then delete the manager-classification requirement: `worker.md:215-222` and
`:258-263` lose the stop-and-report-for-a-clean-rebase text and keep only the
conflict blocker, and `SKILL.md:44-45` stops routing a clean stale-base result
back to main. `scripts.md:142-152` moves from describing caller recovery to
describing the script's own behavior. Root `AGENTS.md:153-157` needs no change
if the confirmation gate is untouched, which it is; confirm that when the prose
is edited.

**B. Require the review handoffs in the dispatch brief.** Extend `SKILL.md`
`## Inputs` so the dispatch brief must carry the typed hygiene and review
handoffs the acceptance table cites — verbatim, or as `Temp/` paths plus
selector, which `.agents/references/subagent-reporting.md:100-107` already
provides for oversized material. This is the smaller of the two candidate fixes.
The alternative — having every reviewer persist its handoff to a known `Temp/`
location so the finalizer reads them without main relaying — changes many skills
and adds a filename convention, and is not recommended unless the fix session
finds that main provably cannot supply them (for example when a handoff was
never in main's context). `landing-acceptance-table.md` needs no rule change:
the scoring bar stays exactly as written; the fix only guarantees the evidence
is present.

**C. Pre-confirmation lock cycles.** Investigate whether the duplicate
claim/release pair survives fix A. It should not: with the rebase inside
preparation there is one pre-confirmation reconciliation pass and therefore one
claim/release cycle. If it does survive, report it; do not weaken or reorder the
lease rules in `scripts.md:189-193` as part of this Plan.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`
  — `:340-346` ancestry guard; tip re-resolution; `:351-353` verified-tree gate.
- `.agents/skills/finalize-changes/references/scripts.md` — `:142-152`
  approval-preparation entry; `:157-167` verified-candidate pair; `:45-49`
  baseline re-resolution.
- `.agents/skills/finalize-changes/references/worker.md` — `:215-222`
  `## Rules`; `:258-263` `## Recovery`; `:66-74` step 2.
- `.agents/skills/finalize-changes/SKILL.md` — `:24-36` `## Inputs`; `:44-45`
  stale-base routing.
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md` —
  read as the unchanged scoring contract.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` —
  `:660-683`, read as the existing deterministic-rebase precedent; not changed.

## In scope

- The clean-rebase behavior of `Invoke-FinalizeApprovalPreparation.ps1` at its
  `git.primary-not-ancestor` ancestry check, the tip re-resolution that follows
  it, the placement of the `-VerifiedCandidateTree` comparison relative to that
  rebase, and the fields its result reports about the rebase.
- The `Invoke-FinalizeApprovalPreparation.ps1` entry in `scripts.md`, rewritten
  from caller recovery to script behavior.
- The stale-base text in `worker.md` `## Rules` and `## Recovery`, and its
  routing sentence in `SKILL.md`.
- The `SKILL.md` `## Inputs` requirement that the dispatch brief carry the typed
  review and hygiene handoffs (verbatim or as `Temp/` path plus selector).
- A reported finding, with evidence, on whether a duplicate pre-confirmation
  lock claim/release cycle remains after the above.

## Out of scope

- The confirmation gate: exactly one explicit user confirmation before primary
  changes, and everything in `SKILL.md` `### Landing confirmation`, stay as
  written.
- Conflicting rebases: they keep returning a blocker to the manager.
- `Invoke-FinalizeLanding.ps1`, the landing lock scripts, and the lease ordering
  rules in `scripts.md:189-193`.
- The scoring rules in `landing-acceptance-table.md`.
- Reviewer-side handoff persistence in other skills, unless part B proves main
  cannot supply the handoffs.
- Backward-compatibility shims for the old caller-performed recovery route:
  none are added; the old route is removed, not kept alongside.

## Acceptance criteria

The diff alone does not prove the behavior, so:

1. With primary advanced by a commit touching no session path, one invocation of
   `Invoke-FinalizeApprovalPreparation.ps1` returns exit 0 with a prepared
   commit on the live primary tip and a result field recording that it rebased —
   no second invocation and no manager message in between.
2. That prepared commit's `git patch-id --stable` equals the pre-rebase session
   commit's, verified in the same run's evidence.
3. With a genuine conflict, the script aborts the rebase, leaves the session
   branch at its pre-rebase commit with a clean `git status --porcelain`, and
   blocks to the manager.
4. `Invoke-FinalizeApprovalPreparation.ps1` still passes its documented
   invocation form unchanged in shape (root `AGENTS.md` bundled-scripts rule),
   and the `-VerifiedCandidateCommit`/`-VerifiedCandidateTree` pair, when
   supplied across a clean rebase, reports `matched` true rather than blocking
   with `candidate.tree-changed`.
5. `/validate-skill` passes on the changed `finalize-changes` package, and
   `/progressive-disclosure-review` reports no unresolved finding on the changed
   instruction prose.

## Notes

Change Workflow tier for the fix: **Tier 2**. The change is scoped behavior of
one skill — its approval-preparation script and its own prose. It touches no
determinism/CRC, wire, serialization, save/replay, or threading surface; it
changes no trust boundary (the confirmation gate and the landing lock are out of
scope) and never advances primary. It does add a Git branch-moving operation to
a script that previously only squashed, inside the session's own worktree, which
is why it is not Tier 1. The fix session should re-confirm the tier against the
root `AGENTS.md` triggers before implementing, and escalate if it finds the
change reaching the landing lock or the primary advance.

Evidence for the timings above is the four `implementer` task notifications main
received during the `94d2bb9d` landing; it is not reconstructible from the
repository and is recorded here rather than cited to a file.
