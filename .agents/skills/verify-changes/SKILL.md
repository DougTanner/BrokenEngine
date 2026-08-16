---
name: verify-changes
description: >-
  Verify a change at a landing gate with a read-only acceptance table bound to
  the reviewed diff and prior role evidence.
allowed-tools: [Read, Grep, Glob, "Bash(git diff *)", "Bash(git status *)", "Bash(git ls-files *)", "Bash(git hash-object *)", "Bash(git submodule status *)", PowerShell]
---

# Verify Changes

Run only at a root [AGENTS.md](../../../AGENTS.md) landing gate. Main dispatches
one fresh read-only `reviewer`; it never edits, delegates, builds, launches
runtime work, changes claims or the tree, or fixes/waives findings — the
`plan validate` run required by `## Executable Plan check` uses `--lint-only`,
so it heals nothing and changes no scheduler state.

## Required inputs

Require the task brief from `../../references/subagent-reporting.md`, plus:

- adopted checkout and route (`session-finalization` or explicitly authorized
  `primary-commit`);
- the final approved Plan or user instruction, plus any changes the user
  approved after it;
- tier/triggers, role assignments, acceptance table, and invariants;
- caller-owned paths and concise ordered handoffs for implementation,
  propagation, checks, reviews, fixes, hygiene, builds, external claims, and
  residuals; and
- the typed artifacts this change set triggers: on a changed `SKILL.md`, the
  complete `/validate-skill` PASS handoff.

Do not accept an unapproved delta or pasted full logs. For a completed claimed
Plan whose file the change deletes, the manager supplies its approved text or
its Git-history path.

## Reviewed diff and authorization

1. Derive the reviewed diff from the read-only inventory: `pwsh -NoProfile -File
   .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <absolute
   repository toplevel> -Baseline <primary-tip full SHA> -Head <session branch
   tip, or the squashed landing commit when one exists> -Landing`. Its
   `landing.reviewed` rows give status, path, old path, and baseline/current
   modes for that three-dot diff, covering additions/deletions, renames,
   symlinks, and gitlinks. That list is diff-derived and contains no untracked
   file, so add the untracked files the caller declared as authorized additions
   and reconcile each of them through `landing.porcelain`, never through
   `landing.reviewed`.
2. Reconcile the diff paths against the caller-declared owned paths. Each
   changed region must name its authorizing Plan clause or user instruction.
   Extra, missing without a recorded no-change decision, or unauthorized bytes block.
3. Confirm every handoff still applies to this diff and no later change
   invalidated it. A fix ends the run: main applies it and re-review covers only
   the changed regions.
4. Inventory ignored/non-worktree state from the same run's `landing.porcelain`
   and `landing.submodules` rows, recording exact path, owner/persistence
   contract, evidence, and `unchanged`, `intentionally persisted`, `restored`,
   or non-passing `residual` outcome. A reviewed path reported dirty whose
   working-tree bytes match the reviewed `-Head` tree is the expected state of an
   uncommitted candidate and classifies as `intentionally persisted`.

The run writes no file. `-Landing` requires a commit-valued `-Head` and excludes
the other modes. Only
`status` `pass` (exit 0) is usable; `blocked` (exit 2) or `error` (exit 1) means
the reviewed diff is unavailable, which is a `BLOCKED` verification, never a
diff rebuilt inline. Each `landing` array is capped at 500 rows: check
`truncation.landing` and `truncated`, and treat an emitted count below the full
count as unreviewed state rather than a clean inventory. A `-Landing` run still
reports the complete top-level `triggers` object like any other run, and
`planTouched` is the only trigger the `landing` object itself adds; no part of
the run ever runs or replaces the WorktreeCli `plan validate` run required by
`## Executable Plan check` below.

## Acceptance table

Return one inline row per approved criterion, invariant, visible changed
behavior, required check/review, and residual:

`criterion | decisive check | PASS | FAIL | BLOCKED | UNVERIFIED | evidence`

Every `PASS` cites evidence this verifier read/ran or a mandated handoff
whose fields prove the row. Narrative alone is `UNVERIFIED`. Duplicate checks
name their independent signal. Tier sets the exploration ceiling: Tier 1 uses
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
`0` — exit `4` is a failed build whose post-build oracle work (verification,
receipt issuance, or state persistence) failed — plus the build-reported `"<mode> receipt verified: path=... sha256=...
dataRoot=... baseline=... aggregate=..."` summary line. Do not accept
schema/result/exit mismatches.

Skill changes require a complete `/validate-skill` PASS handoff with mechanical
self-check, target validator exit/output, semantic review, and no Critical
finding.

## Executable Plan check

When the reviewed diff touches `Documents/Plans/**`, run
`Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe plan
validate --lint-only --repo <absolute Git common directory> --worktree
<adopted checkout>` and require a valid result for the
session worktree. Record notices. Otherwise record `not triggered — no
executable-Plan change`. Primary post-commit validation belongs to
finalization.

`--lint-only` takes no scheduler guard, creates no storage, and heals nothing,
so it reports `healedClaims` as an empty array and the `/codex-review`
read-only sandbox can run it. A run that fails to execute the tool at all is a
blocker: record the row `BLOCKED` with the exact failure, and never substitute
other evidence for the missing run.

## Output

Return `Verification: PASS` only when ownership, authorization, and every row
pass. Otherwise return `Verification: BLOCKED` once with all decisive items; do
not retry any judgment check. A PASS binds the reviewed diff; if that diff
later changes meaningfully, re-review only the changed regions.

A later pass may resume a `BLOCKED` result only when its sole blocking rows are
missing typed artifacts and the baseline and head SHAs are unchanged. That pass
reads the prior report and every citation it would carry forward, re-establishes
the session-change inventory and every row whose evidence is not bound to the
immutable committed diff, and carries forward only rows that evidence binds to
that diff; any identity or state mismatch requires a fresh full verification.
Restate the full table and the baseline and head SHAs it binds.

Follow `../../references/subagent-reporting.md`: route, checkout, the reviewed
diff, Git-derived inventory, acceptance/review/API tables, Plan check,
fix/re-entry history, non-passing items, and `Residuals` last.
