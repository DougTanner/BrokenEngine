<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T13:04:09.132Z","dependsOn":[]} -->
# Fix: /finalize-changes — compare session history from the validated Baseline

## Context

Plan completion leaves the target deletion and direct-child dependency-marker
rewrite uncommitted in the session worktree. Candidate creation is the
mechanism that commits those prepared Plan changes. The session route in
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1:123`
currently calls `Assert-HistoryTreeUnchanged $current $ExpectedPrimaryTip
$ExpectedCurrentTip`; the helper at line 77 diffs the reserved history JSONL
and SVG paths between those tips. When primary alone adds a history-overlay
commit after the session's fork, that comparison falsely attributes the
primary-only bytes to the session patch and blocks with `history.source-changed`.
Pre-candidate rebase is therefore not the canonical fix: it changes the
session context before the operation that is meant to commit the prepared
changes.

`.agents/scripts/AgentWorktreeSession.psm1:104-145` documents and returns
`Baseline` as the session attribution/fork identity. It accepts a recorded
baseline only while it remains on primary history, adjusts it to the detected
divergence after rebases/history rewrites, and falls back through fork-point
resolution before plain merge-base. The implementation should use this
validated value rather than introduce a raw merge-base calculation.

The historical transcript lookup returned structured `transcript.not-found`;
current source and Git evidence settle the mechanism, so no transcript
dependency is required.

## Design

The Plan author's recommendation is to change only the session-landing
comparison base from `ExpectedPrimaryTip` to the already validated and expanded
`Baseline`, making the guard compare `Baseline..ExpectedCurrentTip`. Leave the
shared helper, the primary-advance call site, candidate/rebase/approval
ordering, and all automatic reconciliation behavior unchanged. Candidate
creation then commits the owned prepared changes first; the normal workflow
rebases the clean candidate onto current primary and refreshes context as
already documented.

This preserves the history safety boundary: genuine final-tree changes or
deletions to either reserved path on the session side still block,
dirty reserved paths still block, the primary-advance
`Assert-HistoryTreeUnchanged` call remains unchanged, and no reserved bytes may
land.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1` —
  the `session-landing` comparison argument at line 123; the shared helper and
  primary-advance call are read-only boundaries for this Plan.
- `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1` —
  the smallest matching session-landing fixture additions for primary-only
  history overlays, reserved-path changes/deletions, dirty reserved paths, and
  the unchanged primary-advance route.
- `.agents/skills/finalize-changes/references/scripts.md` — wording only if
  required to document the changed session-route comparison.
- `.agents/skills/finalize-changes/SKILL.md` — read-only and out of scope for
  this Plan; do not modify it.
- `.agents/scripts/AgentWorktreeSession.psm1` — evidence source only; do not
  modify it for this Plan.

## In scope

- Change the `session-landing` `Assert-HistoryTreeUnchanged` base argument in
  `Invoke-FinalizeCandidateCommit.ps1` from `ExpectedPrimaryTip` to `Baseline`.
- Add the smallest matching topology fixtures in
  `Test-FinalizeWorkflowFixtures.ps1` and update `references/scripts.md` only
  if its wording becomes inaccurate.
- Preserve existing dirty-path, primary-advance, candidate/rebase, approval,
  landing, lock, history-generation, and automatic-reconciliation behavior.

## Out of scope

- Changes to `Assert-HistoryTreeUnchanged` shared-helper semantics, the
  primary-commit or primary-advance call, approval/landing/locks, the history
  generator, automatic stash/rebase/retry, scheduler behavior, or unrelated
  documentation/scripts.
- Changes to `.agents/scripts/AgentWorktreeSession.psm1`; it supplies the
  validated `Baseline`.
- No change to `finalize-changes/SKILL.md` in this Plan; it is unconditionally
  read-only and out of scope.
- Unit tests, transcript paths, transcript text, or home paths.

## Risk tier and invariants

Expected Tier 2 only when implementation is isolated to the session-route
comparison argument plus targeted fixtures and any matching reference wording.
The Plan author recommends escalating to Tier 3 if the change reaches shared
helper semantics, primary-advance/history-overlay/landing behavior, or automatic
reconciliation. The reserved JSONL/SVG paths remain generator-owned: session
modification/deletion and dirty worktree/index state must still block, and no
reserved bytes may land.

## Acceptance criteria

- A targeted fixture where primary alone adds reserved history bytes while the
  session changes only normal owned paths reaches `candidate.created`.
- The same topology with a session-side reserved-path modification or deletion
  blocks with `history.source-changed`.
- Dirty reserved worktree or index state still blocks with
  `history.source-dirty`.
- The primary-advance fixture and route remain unchanged.
- `/validate-skill` passes for any changed skill file and plan validate exits 0
  with `status: valid` and `code: ok`.
- No unit tests are added.
