<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T18:23:16.900Z","dependsOn":[]} -->
# Fix: finalize-changes — fresh `/verify-changes` PASS has no retained artifact paths

## Context

During finalization of the scheduler-hash change, the required fresh
`/verify-changes` reviewer returned `PASS` inline through collaboration, but the
handoff did not retain or provide the verification prompt and output file paths.
The manager had to manually materialize the ignored
`Temp/verify-scheduler-hash-prompt.md` and
`Temp/verify-scheduler-hash-output.md` files before step 4 could invoke
`Show-FinalizeApprovalReview.ps1`.

The documented launch command was the `Show-FinalizeApprovalReview.ps1`
invocation in `.agents/skills/finalize-changes/references/scripts.md:72`, with
`-LaunchSmartGit`, `-VerificationPromptFile`, and `-VerificationOutFile` for
approved tip `7f2aec39781ebfaf2277bf702fb717a0e9fe7c59`. The first invocation
returned the exact `broken-engine-finalize-approval-review/v1` envelope with
exit `1`, status `error`, and code `verification.tip-mismatch` because the
prompt lacked the generated `Head: 7f2aec39781ebfaf2277bf702fb717a0e9fe7c59`.
The second returned the same exit, status, and code because `Head:` was outside
the required `# (c) Evidence` section. The manager then recreated the
four-section prompt, restored the reviewer-role line, full head, terminal
`PASS`, and output freshness; the third invocation returned exit `0`, status
`opened`, code `ok`, and opened SmartGit.

The required handoff is documented at
`.agents/skills/finalize-changes/SKILL.md:61-69` and
`.agents/skills/finalize-changes/references/workflow.md:115-123`. The finalizer
currently enforces the file paths and exact evidence shape at
`.agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1:9-18,42-46,128-191`,
and the fixture helper constructs that shape at
`.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1:833-841`.
The Codex review contract says the output remains on disk and the caller
returns its path at `.agents/skills/codex-review/SKILL.md:121-140`. The observed
inline-only handoff and the resulting manual file reconstruction are the
friction; this Plan does not claim a root cause before `/next-plan-review`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: e710ce40-c68d-4429-b03b-73c9baa02a9e
- Session branch: codex/e710ce40-c68d-4429-b03b-73c9baa02a9e
- Worktree: .codex\worktrees\BrokenEngine\e710ce40-c68d-4429-b03b-73c9baa02a9e
- Landing ref: codex/e710ce40-c68d-4429-b03b-73c9baa02a9e
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

In a new session, run `/next-plan-review codex/e710ce40-c68d-4429-b03b-73c9baa02a9e`,
supplying client `codex` and that review ref only. Root-cause the friction from
the proven run, then make the smallest fix inside the `## In scope` boundary
below. If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

As a starting hypothesis, not a decision, the author recommends making the
already-generated prompt path and reviewer-output path an explicit retained
handoff for every completed `/verify-changes` PASS, then passing those exact
paths to finalize step 4. This reuses the existing path and evidence contracts,
eliminates manual reconstruction, and leaves the reviewer verdict, full-head
binding, and finalizer checks unchanged. The reviewing session should confirm
or replace this hypothesis from the run and the current contracts.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md:61-69` — manager-to-finalizer
  prompt/output handoff.
- `.agents/skills/finalize-changes/references/workflow.md:115-123` — step-4
  invocation and required receipt inputs.
- `.agents/skills/finalize-changes/references/scripts.md:215-252` — receipt,
  verification-file, and exact-tip contracts.
- `.agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1:9-18,42-46,128-191`
  — launch inputs and evidence checks.
- `.agents/skills/codex-review/SKILL.md:42-55,87-140` — prompt creation,
  output retention, and reviewer handoff.
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:1-6,56-101`
  — generated prompt path and receipt publication.

## In scope

- Root-cause investigation via `/next-plan-review`, run with client `codex` and
  the landing ref named in `## Design`.
- The smallest resulting fix, confined to the critical files above: preserve
  the existing `/verify-changes` PASS and exact-head evidence requirements,
  while ensuring the generated prompt and reviewer-output paths survive the
  reviewer-to-finalizer handoff and are usable by the documented step-4
  invocation.
- Updating only the named finalize handoff/receipt prose and Codex prompt or
  output-retention regions needed by the proven root cause. If the fix requires
  a file or transport outside this boundary, return for re-planning.

## Out of scope

- The landed scheduler change, including `PlanScheduler.cpp`,
  `Tools/WorktreeCli/AGENTS.md`, and the deleted
  `Documents/Plans/Agents/SchedulerHashFailureFailClosed.md`.
- Any change to `/verify-changes` acceptance criteria, reviewer verdict
  vocabulary, full-head binding, or reviewer assignment.
- SmartGit discovery or launch semantics, approval-review identity, landing
  lock/CAS, primary movement, explicit confirmation, claim cleanup, or the
  finalizer's trust boundary.
- The existing `RemoveFinalizeVerificationArtifactRevalidation.md` Plan,
  unrelated skills or scripts, unit tests, and any transcript path or transcript
  text. A root cause requiring files outside the critical-file list is surfaced
  for re-planning.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior: reviewer artifact retention and the
finalizer handoff). Escalate if the fix changes approval trust semantics or
reaches build/bootstrap coordination. Preserve these invariants:

- A fresh `/verify-changes` PASS remains mandatory and precedes SmartGit,
  summary, and confirmation.
- The finalizer continues to require the exact approved tip in the generated
  evidence, the reviewer-role line, a terminal `PASS`, and output newer than the
  prompt; no check is weakened to hide the handoff failure.
- Prompt and output paths identify the generated artifacts retained for that
  review; no manually authored substitute or stale artifact satisfies the
  gate.
- This tooling change exposes no simulation determinism/CRC, serialization,
  replay, wire, `.pack`, threading, allocation, shader, or runtime-game
  invariant.

## Coordination

`Documents/Plans/Agents/RemoveFinalizeVerificationArtifactRevalidation.md`
independently removes the finalizer prompt/output handoff and
verification-artifact revalidation that this Plan's retention approach
preserves. Both Plans are independent and neither depends on the other.
Whichever Plan executes must re-derive the current finalize and reviewer
contracts and reconcile the other Plan's assumptions with the current tree,
rather than silently invalidating the other Plan.

## Acceptance criteria

- A fresh `/verify-changes` PASS supplies retained, usable prompt and output
  paths to finalize step 4, and the documented
  `Show-FinalizeApprovalReview.ps1 -LaunchSmartGit` invocation succeeds for the
  exact approved tip without manual file materialization or prompt repair.
- The generated prompt still has the required four sections and evidence head,
  and the finalizer still enforces the reviewer-role line, exact head, terminal
  `PASS`, and output freshness.
- The Codex review handoff contract identifies the owner and lifetime of both
  artifacts and returns their paths alongside the inline result, including the
  completed `PASS` path used by finalize.
- `/validate-skill` passes for any changed `SKILL.md`,
  `/progressive-disclosure-review` passes for changed instruction prose, and
  WorktreeCli `plan validate` exits `0` with `status: valid` and `code: ok`.
- No existing Plan is edited and no unrelated tracked path is introduced.

## Notes

- This Plan has no dependency and is independently executable.
- The current recording stage creates only this executable Plan and does not
  change finalization, reviewer, or scheduler behavior.
