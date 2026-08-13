# Finalize Landing Blocked by the Permission Classifier When a Subagent Runs It

Status: findings record. It lives in `Documents/Investigations/` because the cause is not yet known and no remedy can be chosen, so it is never a scheduler input and carries no `broken-engine-plan/v1` metadata marker. It becomes a Plan once diagnosis identifies the mechanism and it moves to `Documents/Plans/<area>/` with byte-zero metadata.

## What was observed

During the landing of `cba1351fc9f2b844e883e345b8113da5c1f54b32`, a delegated `implementer` performing `/finalize-changes` step 5 invoked `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` with its full documented argument set (`-CurrentWorktree`, `-PrimaryWorktree`, `-CurrentBranch`, `-PrimaryBranch`, `-ExpectedCurrentTip`, `-ExpectedPrimaryTip`, `-SessionLabel`, `-ApprovedSessionCommit`, `-ApprovedCandidateTree`, `-OwnerToken`).

Both attempts — one through the PowerShell tool and one through the Bash tool — returned `Blocked by classifier` with no script output. The script never executed and changed nothing.

Main then ran the identical documented command itself and it succeeded on the first attempt: `status: landed`, `primaryAdvanced: true`, `treeVerified: true`, `rebaseAttempts: 0`.

The worker correctly refused to improvise. `.agents/skills/finalize-changes/SKILL.md:39-43` forbids wrapping, replacing, or reconstructing these scripts by hand, and hand-rolling the landing would bypass the compare-and-swap primary advance and the rollback that make changing primary safe.

## What is known

- The block is not a script failure: it occurs before the script runs, so the landing state machine, lock lease, and guards were never reached.
- It is not shell-specific: PowerShell and Bash invocations were blocked identically.
- It is not argument-specific in any way the evidence shows: main's invocation carried the same arguments and was allowed.
- The documented invocation and result contract are `.agents/skills/finalize-changes/references/scripts.md:34`.

## What is not yet known

Why the classifier treats the same command differently depending on whether a delegated subagent or the main session issues it. Nothing observed distinguishes the two invocations except the caller.

## Why it matters

Dispatching a worker for `/finalize-changes` is the documented normal route, so the block sits on the standard landing path of every session, and it appears only at the final step, after all review effort and the user's one landing confirmation have already been spent. The workaround — main invoking landing directly — contradicts the skill's own delegation model, so it is not an acceptable steady state.

## What a future diagnosis would need to establish

1. A reproduction: invoke `Invoke-FinalizeLanding.ps1` from a delegated subagent with its documented arguments. A dry, non-landing reproduction suffices, because the block precedes execution.
2. The exact classifier input and decision for the blocked invocation, and how it differs from main's allowed one — caller identity, working directory, script path form, or argument text.
3. Whether any repository-owned surface can change the outcome. `.claude/settings.json` is the repository-level Claude configuration that would carry a permission rule if a rule turns out to be the mechanism; if the decision is not repository-configurable, that is itself the finding.
4. Whether the resulting mechanism can stay narrow enough that it does not authorize arbitrary script execution, and leaves the landing script's own guards — lock lease, compare-and-swap advance, rollback, rebase retry, tree verification, claim release — untouched.

Any remedy must remain a single documented invocation: no wrapper, retry loop, alternative landing path, or main-session workaround.

## Out of scope

The landed change `cba1351fc9f2b844e883e345b8113da5c1f54b32` and its content; landing behavior itself; `.agents/skills/finalize-changes/SKILL.md` normal-workflow and recovery prose, whose edits are owned by `Documents/Plans/Agents/CodexReviewMetricsCompareSandbox.md` and `Documents/Plans/Agents/FinalizeCandidateResumedOwnedPaths.md`; and the other finalize scripts (candidate commit, approval preparation, approval review), which are different scripts with different symptoms.

## Notes

This record is keyed to the pair (`Invoke-FinalizeLanding.ps1`, `Blocked by classifier` from a delegated agent). A later observation of the same pair is a duplicate, not a new residual. No Plan under `Documents/Plans/Agents/` mentions classifier blocking.
