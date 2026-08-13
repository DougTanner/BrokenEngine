<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-13T12:19:59.876Z","dependsOn":[]} -->
# Fix: New-PlanFile.ps1 reports scheduler guard failures as invalid plan metadata

## Context

`.agents/scripts/New-PlanFile.ps1` folds a WorktreeCli `plan validate` run into its result. Every
non-passing validation collapses into one outcome at `:191-200`:

```powershell
Complete-PlanFile $exit $(if ($exit -eq 2) { 'blocked' } else { 'error' }) 'plan.validation-failed' "Plan file was written but 'plan validate' rejected the tree; fix the body and revalidate."
```

That advice is wrong for a scheduler lock failure, because the written Plan body is fine and editing it
changes nothing. Two of `plan validate`'s codes are lock outcomes, not metadata outcomes
(`Tools/WorktreeCli/PlanScheduler.cpp:516-521`, `Tools/WorktreeCli/AGENTS.md` "Coordination State"):

- `busy` with exit `2` — another session held the scheduler guard for the full wait; the correct action is to
  tell the user and retry later.
- `guard-unavailable` with exit `1` — the guard's lock file or its storage is unusable; re-running cannot
  help.

Both currently arrive at the caller as `plan.validation-failed` with the "fix the body" message, so a caller
that follows the returned advice edits a correct Plan body in response to a lock problem.

This is the same defect class already fixed in the sibling claim script; its corrected shape is the
reference for this fix (`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:24-25`), which maps the
same two codes to `scheduler.busy` (exit 2, blocked) and `scheduler.guard-unavailable` (exit 1, error) before
falling through to `plan.validation-failed`.

This was proven while fixing the claim script and was deliberately left out of that change as scope
expansion: `New-PlanFile.ps1` is a different script with its own result contract.

## Design

In `New-PlanFile.ps1`, before the existing `plan.validation-failed` completion at `:191-200`, add the two
code-specific completions, keyed on the folded validation projection the script already builds
(`$projection['code']`, `$validate.ExitCode`), using ordinal case-sensitive comparison:

- `$validate.ExitCode -eq 2` and code `busy` — `Complete-PlanFile 2 'blocked' 'scheduler.busy'` with a message
  stating another session held the plan scheduler for the full wait, the Plan file was written and left
  staged, its body is not the problem, and revalidation can be retried later.
- `$validate.ExitCode -eq 1` and code `guard-unavailable` — `Complete-PlanFile 1 'error'
  'scheduler.guard-unavailable'` with a message stating the plan scheduler lock storage is unusable, the Plan
  file was written and left staged, this is not a Plan metadata problem, and re-running will not help.

Everything else is unchanged: the written Plan stays on disk and staged in both new paths exactly as today,
`written` still reports file existence independently of the exit code, the diagnostics line already emitted
for a non-zero exit still runs, and every other failure keeps `plan.validation-failed` with its current
message. Exit codes keep their established meaning (`2` state conflict, `1` OS/transport failure), so no
caller's exit handling changes.

`.agents/references/new-plan-file.md` documents the result contract and currently names only the exit
meanings, so extend its `## Result` section with the two new codes and the fact that they leave the written
Plan in place and are not body problems.

## Critical files

- `.agents/scripts/New-PlanFile.ps1` — the `$validationPassed` failure block at `:191-200`.
- `.agents/references/new-plan-file.md` — `## Result`.
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:24-25` — reference shape only; not changed.

## In scope

- The `$validationPassed` failure block in `New-PlanFile.ps1`: the two added code-specific completions and
  their messages.
- The `## Result` section of `.agents/references/new-plan-file.md` describing those two codes.

## Out of scope

- WorktreeCli, `Tools/WorktreeCli/PlanScheduler.cpp`, and the codes `plan validate` itself emits.
- `Invoke-NextPlanClaim.ps1` and any other next-plan workflow script.
- Marker minting, timestamps, encoding, dependency handling, filename rules, overwrite refusal, and staging in
  `New-PlanFile.ps1`.
- Retry, wait, or recovery behavior: neither new path retries anything.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior; one script's result contract plus its reference document). The result
envelope stays a single `broken-engine-new-plan-file/v1` JSON object on stdout with the established exit-code
meanings, and a written Plan is never deleted or rewritten by a failure path.

## Acceptance criteria

- A `plan validate` result of exit `2` with code `busy` makes `New-PlanFile.ps1` exit `2` with
  `status: blocked`, `code: scheduler.busy`, and a message that does not tell the caller to fix the body; the
  Plan file remains on disk and staged, with `written: true`.
- A `plan validate` result of exit `1` with code `guard-unavailable` makes the script exit `1` with
  `status: error`, `code: scheduler.guard-unavailable`, and a message identifying unusable lock storage; the
  Plan file remains on disk and staged.
- A genuine metadata failure (a body that fails the lint) still exits non-zero with
  `code: plan.validation-failed` and the existing message.
- A successful creation still exits `0` with `status: pass`, `code: ok`, and nested validation
  `exitCode: 0`, `status: valid`, `code: ok`.
