# Landing acceptance table

Contract for the acceptance table the finalizer fills on the prepared landing
diff, before main asks the confirmation.

## Reviewed diff and authorization

1. Derive the reviewed diff from the read-only inventory: `pwsh -NoProfile -File
   .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <absolute
   repository toplevel> -Baseline <primary-tip full SHA> -Head <session branch
   tip, or the squashed landing commit when one exists> -Landing`. Its
   `landing.reviewed` rows give status, path, old path, and baseline/current
   modes for that three-dot diff, covering additions/deletions, renames,
   symlinks, and gitlinks. That list is diff-derived and contains no untracked
   file, so add the untracked files declared as authorized additions and
   reconcile each of them through `landing.porcelain`, never through
   `landing.reviewed`.
2. Reconcile the diff paths against the declared owned paths. Each changed
   region must name its authorizing Plan clause or user instruction. Extra,
   missing without a recorded no-change decision, or unauthorized bytes block.
3. Confirm every handoff still applies to this diff. A later change invalidates
   a handoff only where it changed bytes that handoff's result depends on — for
   the two typed prose handoffs below, the session's own bytes in the regions
   they read.
4. Inventory ignored/non-worktree state from the same run's `landing.porcelain`
   and `landing.submodules` rows, recording exact path, owner/persistence
   contract, evidence, and `unchanged`, `intentionally persisted`, `restored`,
   or non-passing `residual` outcome. A reviewed path reported dirty whose
   working-tree bytes match the reviewed `-Head` tree is the expected state of an
   uncommitted candidate and classifies as `intentionally persisted`.

The run writes no file. `-Landing` requires a commit-valued `-Head` and excludes
the other modes. Only `status` `pass` (exit 0) is usable; `blocked` (exit 2) or
`error` (exit 1) means the reviewed diff is unavailable, which is a `BLOCKED`
table, never a diff rebuilt inline. Each `landing` array is capped at 500 rows:
check `truncation.landing` and `truncated`, and treat an emitted count below the
full count as unreviewed state rather than a clean inventory. A `-Landing` run
still reports the complete top-level `triggers` object like any other run, and
`planTouched` is the only trigger the `landing` object itself adds; no part of
the run ever runs or replaces the WorktreeCli `plan validate` run required by
`## Executable Plan check` below.

## Acceptance table

Return one inline row per approved criterion, invariant, visible changed
behavior, required check/review, and residual:

`criterion | decisive check | PASS | FAIL | BLOCKED | UNVERIFIED | evidence`

Every `PASS` cites evidence read or run here, or a mandated handoff whose fields
prove the row. Narrative alone is `UNVERIFIED`. Duplicate checks name their
independent signal. Tier sets the exploration ceiling: Tier 1 uses
static/schema/link/validator/affected-target compile evidence; Tier 2 adds the
approved observable scenario; Tier 3 adds exposed invariant/integration
evidence. Only an approved user delta may revise/defer a criterion, and a
deferral names its tracked Plan.

Reconcile each review as:
`review | delegated/inline | findings | accepted-fixed | refuted | unresolved | evidence`.
Require zero unresolved accepted findings and fix/recheck evidence. External
claims name a single checkable proposition, version/configuration, official
primary source, verdict, dependent finding, and evidence; unresolved or
undecided refutations are non-passing.

For builds, require the authoritative `broken-engine-build-result/v1`: intended
target/selection/arguments, status/exit/failure kind, complete retained log,
decisive diagnostics, and all requested targets. Game builds additionally
require data mode, generation authority, Gaea outcome, and normalized paths;
their data evidence is that same envelope reporting success with wrapper exit
`0`. Do not accept schema/result/exit mismatches.

Skill changes require a complete `/validate-skill` handoff with mechanical
self-check, target validator exit/output, semantic review, and no unresolved
Critical finding.

Instruction-doc changes require a complete `/progressive-disclosure-review`
handoff with the baseline, the files checked, the findings, and no unresolved
finding.

A missing typed artifact — `/validate-skill`, `/progressive-disclosure-review`,
or a build envelope — is a `BLOCKED` row, never a `PASS`.

A `NEEDS_ACTION` typed handoff still scores its row when every finding it raised
is resolved — fixed in the reviewed diff or recorded refuted per the review
reconciliation above — and a scoped spot-check re-review of exactly those
regions returned `PASS`. Never dispatch a repeated whole-artifact review for
that row. For `/validate-skill` the mechanical evidence is then that skill's
documented validator runs on the reviewed tree, script runs rather than a
review.

## Executable Plan check

When the reviewed diff touches `Documents/Plans/**`, run
`Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe plan
validate --lint-only --repo <absolute Git common directory> --worktree
<adopted checkout>` and require a valid result for the session worktree. Record
notices. Otherwise record `not triggered — no executable-Plan change`.
`--lint-only` takes no scheduler guard, creates no storage, and heals nothing,
so it reports `healedClaims` as an empty array and changes no scheduler state.
A run that fails to execute the tool at all is a blocker: record the row
`BLOCKED` with the exact failure, and never substitute other evidence for the
missing run.
