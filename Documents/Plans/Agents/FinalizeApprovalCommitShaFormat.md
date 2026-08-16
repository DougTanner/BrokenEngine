<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T17:04:54.005Z","dependsOn":[]} -->
# Fix: finalize-changes scripts reference — commit parameters reject abbreviated SHAs undocumented

## Context

During `/finalize-changes` for the claimed Plan
`Documents/Plans/Agents/CompileDataOracleVerifierHandoff.md`, the finalizer ran
the documented approval-preparation command
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`
with abbreviated (short) commit SHAs supplied for `-ExpectedCurrentTip` and
`-ExpectedPrimaryTip`. The script exited with code `input.invalid` and message
"Commit inputs must be exactly 40 lowercase hexadecimal characters", producing no
prepared candidate. The invocation was then repeated with full 40-character SHAs
and succeeded, so the friction cost one wasted invocation of a landing-path
script.

The rejection is enforced at
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1:269-271`,
which loops over `$ExpectedCurrentTip` and `$ExpectedPrimaryTip` and asserts
`-cmatch '^[0-9a-f]{40}$'`. The reference that documents the invocation,
`.agents/skills/finalize-changes/references/scripts.md`, shows the
approval-preparation command with placeholders `'<current-tip>'` and
`'<primary-tip>'` at `:17` (the same placeholders appear for the candidate-commit
command at `:16` and the landing command at `:20`), and its only surrounding
input guidance at `:13` is "Angle-bracket values are placeholders; quote every
one." Nothing in that file states that a commit-valued parameter must be exactly
40 lowercase hexadecimal characters, so a short SHA looks acceptable from the
documentation alone.

The claimed Plan executed this session confined its `## In scope` to
`.agents/skills/verify-changes/SKILL.md`,
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`, and
`Test-CodexReviewPromptFixtures.ps1`, and its `## Out of scope` explicitly named
"Unrelated skills/scripts". The finalize-changes scripts reference is therefore
outside the active change and not an in-scope blocker of it.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: a6d2ff7c-2bf7-4a97-bcd6-093439653cfc
- Worktree/branch UUID: 932f63ce-923c-400e-bc54-1cd08fb629a4
- Session branch: claude/932f63ce-923c-400e-bc54-1cd08fb629a4
- Worktree: .claude\worktrees\BrokenEngine\932f63ce-923c-400e-bc54-1cd08fb629a4
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID; root-cause the friction from the
proven transcript, then make the smallest fix inside the `## In scope` boundary
below. If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The fix direction is already fixed: state the exactly-40-lowercase-hexadecimal
requirement for commit-valued parameters in
`.agents/skills/finalize-changes/references/scripts.md` where those parameters
are documented — the `## Invocation` section that carries the placeholder text
and the commands using `-Baseline`, `-ExpectedCurrentTip`, `-ExpectedPrimaryTip`,
`-ApprovedSessionCommit`, `-ApprovedTip`, and `-LandedCommit`. Documentation
only; the scripts' existing validation behavior stays byte-for-byte unchanged,
and abbreviated SHAs must keep being rejected.

Verify every cited line number against the working tree before editing — the
line numbers above are from this session and the files may have moved since.

## Critical files

- `.agents/skills/finalize-changes/references/scripts.md` — the `## Invocation`
  placeholder guidance (`:13`) and the commands documenting commit-valued
  parameters (`:16-23`); this file is the authorized fix boundary
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`
  — read-only reference for the enforced format (`:269-271`); not edited

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref, client, and conversation session ID
- The smallest resulting documentation fix, confined to the `## Invocation`
  section of `.agents/skills/finalize-changes/references/scripts.md` (and its
  `## Contracts` section only if the review proves the requirement belongs
  there), stating the exactly-40-lowercase-hex requirement for commit-valued
  parameters

## Out of scope

- Any change to the scripts' input validation, error codes, messages, or
  acceptance of abbreviated SHAs
- `.agents/skills/finalize-changes/SKILL.md` and every other finalize reference,
  unless the review proves the same undocumented requirement misleads a reader
  there
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 1 (documentation only); escalate to Tier 2 if the review's proven
fix changes script behavior, and to Tier 3 if it reaches landing or
build/bootstrap coordination. The canonical invocation form stays byte-identical,
and commit-input validation stays fail-closed. Never embed transcript paths or
home paths.

## Acceptance criteria

- `.agents/skills/finalize-changes/references/scripts.md` states the exactly-40
  lowercase-hexadecimal requirement for the commit-valued parameters it
  documents, so the recorded symptom is no longer reachable from the
  documentation alone
- The scripts' commit-input validation is unchanged: an abbreviated SHA still
  exits with `input.invalid` and its existing message
- `plan validate` exits `0` with `status: valid` and `code: ok`;
  `/validate-skill` runs only if a `SKILL.md` changes

## Notes

This Plan is keyed to the concrete (script/reference, symptom) pair: the
documented finalize approval-preparation invocation gives no indication that
commit-valued parameters reject abbreviated SHAs, so a short SHA costs a failed
invocation with `input.invalid`. A later observation of the same pair is a
duplicate, not a new residual. `Documents/Plans/Agents/FinalizeApprovalReviewInvocationContract.md`
covers a different script and a different symptom (the documented
`Show-FinalizeApprovalReview.ps1` form omits mandatory parameters entirely), so
it is not a duplicate of this one. The proven root cause is deferred to
`/next-plan-review`.
