# Landing acceptance table

Contract for the acceptance table the finalizer fills on the prepared landing
diff, before main asks the confirmation.

## Reviewed diff and authorization

1. Derive the reviewed diff from the read-only inventory: `pwsh -NoProfile -File
   .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot '<absolute
   repository toplevel>' -Baseline <primary-tip full SHA> -Head <session branch
   tip, or the squashed landing commit when one exists> -Landing -OutputPath
   Temp/finalize-landing-inventory.json`, passing `-SavePlanPaths
   '<comma-separated paths>'` when the brief carries `Save-plan paths`. Read
   that JSON result file. Its `landing.reviewed` rows give status, path, old
   path, and baseline/current modes for that three-dot diff, covering
   additions/deletions, renames, symlinks, and gitlinks. That list is
   diff-derived and contains no untracked file, so add the untracked files
   declared as authorized additions and reconcile each of them through
   `landing.porcelain`, never through `landing.reviewed`.
2. Reconcile the diff paths against the declared owned paths, the
   `Projects/Coretide/` diff paths against the Coretide landing set. Each
   changed region must name its authorizing Plan clause or user instruction,
   and beside it every accepted review finding ID (Plan review,
   `/external-grill-plan`, or `/resolve-findings`) and every user decision (the
   `/plan-alternatives` pick, an `/external-grill-plan` decision ID, or an
   approved user delta) that prompted or chose the change, read from the
   records the brief carries. A finding ID or decision alone authorizes
   nothing. Extra, missing without a recorded no-change decision, or
   unauthorized bytes block.
3. Confirm every handoff still applies to this diff. The review-round check of
   the `-Landing` run decides whether review handoffs still apply; a later
   change invalidates any other handoff only where it changed bytes that
   handoff's result depends on.
4. Inventory ignored/non-worktree state from the same run's `landing.porcelain`
   and `landing.submodules` rows, recording exact path, owner/persistence
   contract, evidence, and `unchanged`, `intentionally persisted`, `restored`,
   or non-passing `residual` outcome. A reviewed path reported dirty whose
   working-tree bytes match the reviewed `-Head` tree is the expected state of
   an uncommitted candidate and classifies as `intentionally persisted`. The
   ignored `Projects/Coretide/` entry is the session Coretide clone, whose
   changes the reviewed rows carry, and classifies as `intentionally
   persisted`.

The run writes only the named result file and emits one summary line on stdout.
`-Landing` requires a commit-valued `-Head` and excludes the other modes. Only
`status` `pass` is usable; any other status means the reviewed diff is
unavailable, which is a `BLOCKED` table, never a diff rebuilt inline. A
`blocked` status with an `inventory.review-round-*` code is the review-round
check (`Test-ReviewRoundCoverage` in
`.agents/scripts/SessionChangeInventory.psm1`) refusing the candidate because a
round is open or owed; main reruns the step script, whose next round covers it
(a `/sweep` run: [`/sweep`](../../sweep/SKILL.md) step 11). The code
`inventory.round-evaluation-failed` is not that refusal: the rounds could not
be evaluated at the candidate, no further round cures it, and both landing
scripts report it with the `terminal` `disposition`
([`scripts.md`](scripts.md#landing-and-recovery)). Treat
`truncated` true as unreviewed state rather than a clean inventory. A
`-Landing` run still reports the complete top-level `triggers` object like any
other run, and `planTouched` is the only trigger the `landing` object itself
adds; no part of the run ever runs or replaces the WorktreeCli `plan validate`
run required by `## Executable Plan check` below. The `landing` object also
carries `acceptanceSkeleton`: the rows this landing owes, each emitted
`BLOCKED` and named by skill, plus the `Executable Plan check` row when the
reviewed diff touches `Documents/Plans/` or `Projects/Coretide/Documents/Plans/`.

## Acceptance table

Return one inline row per approved criterion, invariant, visible changed
behavior, required check/review, and residual:

`criterion | decisive check | PASS | FAIL | BLOCKED | UNVERIFIED | evidence`

Exempt paths owing no Review and resolve correctness row are the entries
`Test-ReviewExemptEntry` (`.agents/scripts/SessionChangeInventory.psm1`)
accepts; a deleted Plan among them still gets `## Executable Plan check`.

On every diff — `/save-plan`-only or mixed (including post-review or
recovery-added follow-up Plans) — require `Invoke-StaticChecks.ps1` `PASS` and
run `## Executable Plan check` whenever emitted; mixed diffs also require the
passing review-round check plus the handoffs of the rounds' reviews.

Every `PASS` cites evidence read or run here, or a mandated handoff whose fields
prove the row. Narrative alone is `UNVERIFIED`. Duplicate checks name their
independent signal. The change's tier sets the exploration ceiling, per
[`../../verify-acceptance/references/worker.md`](../../verify-acceptance/references/worker.md)
`## Tier evidence ceiling`; at the landing gate Tier-1 compile evidence covers
every affected target and the Tier-2 scenario is the approved one. Only an
approved user delta may revise/defer a criterion, and a
deferral names its tracked Plan.

Reconcile each review as:
`review | delegated/inline | findings | accepted-fixed | filed | dropped | refuted | unresolved | evidence`.
`filed` and `dropped` are the accepted findings a `-CloseRound` result of
`.agents/scripts/Get-ChangeWorkflowSteps.ps1` says to file or to leave unfixed,
a `filed` one naming its `/create-follow-up-plans` Plan path in the reviewed
diff. Require zero unresolved accepted findings and fix/recheck evidence.
External claims name a single checkable proposition, version/configuration,
official primary source, verdict, dependent finding, and evidence; unresolved
or undecided refutations are non-passing.

For builds, require the authoritative `broken-engine-build-result/v1`: intended
target/selection/arguments, status/exit/failure kind, complete retained log,
decisive diagnostics, and all requested targets. A game build additionally
requires the complete `/compile` handoff for that build and the envelope file
it cites, reporting success. Do not accept schema/result/exit mismatches.

Skill changes require a complete `/external-skill-creator` findings-only
validate-mode handoff with mechanical self-check, target validator exit/output,
semantic review, and no unresolved Critical or High finding.

Instruction-doc changes require a complete `/progressive-disclosure-review`
handoff with the baseline, the files checked, the findings, and no unresolved
finding.

A missing typed artifact — `/external-skill-creator` validate mode,
`/progressive-disclosure-review`, or a build envelope — is a `BLOCKED` row,
never a `PASS`.

A `NEEDS_ACTION` typed handoff still scores its row when every finding it raised
is fixed in the reviewed diff, filed, dropped, or recorded refuted per the
review reconciliation above, and the review-round check passed. For
`/external-skill-creator` validate mode the mechanical evidence is then that
skill's documented validator runs on the reviewed tree, script runs rather than
a review.

## Executable Plan check

For the `Executable Plan check` row the landing receipt's `acceptanceSkeleton`
emits, run `Tools/WorktreeCli/Platforms/VisualStudio2026/Output/WorktreeCli.exe`
with `plan validate --lint-only --repo '<absolute Git common directory>'
--worktree '<adopted checkout>'`, requiring JSON `status: valid` and recording
notices. When the landing updates `WorktreeCli`'s Plan metadata schema before
promotion, you must run the verified `Temp/AgentToolsCandidate/WorktreeCli.exe`
candidate instead, and record the row `BLOCKED` if that candidate is
unavailable. `--lint-only` takes no scheduler guard, creates no storage, and
heals nothing, so it reports `healedClaims` as an empty array and changes no
scheduler state. A run that fails to execute the tool at all is a blocker:
record the row `BLOCKED` with the exact failure, and never substitute other
evidence for the missing run.
