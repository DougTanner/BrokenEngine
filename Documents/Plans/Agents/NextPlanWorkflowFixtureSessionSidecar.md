<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-13T12:20:08.445Z","dependsOn":[]} -->
# Fix: Test-NextPlanWorkflowScripts.ps1 — fixture session worktree has no session sidecar

## Context

`.agents/skills/next-plan/SKILL.md:125-128` makes this suite mandatory whenever a next-plan workflow script
changes. Run as documented:

```powershell
pwsh -NoProfile -File .agents/skills/next-plan/scripts/Test-NextPlanWorkflowScripts.ps1 -Executable '<provisioned WorktreeCli path>'
```

it fails at its first case. `Defer-NextPlan.ps1` exits `2` with `code: "defer.context-conflict"`, and the
message reports that the session sidecar `Temp\session-sidecar.json` is missing, so no case after it runs.

The cause is in the fixture, not in the scripts under test. The fixture creates its session worktree on a
session-shaped branch, `codex/<guid>`
(`.agents/skills/next-plan/scripts/Test-NextPlanWorkflowScripts.ps1:82-83`), but never writes the sidecar file.
`.agents/scripts/AgentWorktreeSession.psm1:75-101` hard-requires that file for any branch matching
`^(?:claude|codex)/<uuid>$`: it throws when the file is missing (`:79-81`), and deliberately does not fall back
to a live primary-branch lookup, because a session lands onto its recorded parent. Every next-plan workflow
script resolves its context through that module, so the very first script invocation throws.

This is proven pre-existing: the identical failure reproduces against the pre-session primary
`WorktreeCli.exe`, so no session change caused it.

Impact: the mandatory suite for these scripts cannot execute at all, so the two scheduler-lock branches added
at `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:24-25` (`scheduler.busy` and
`scheduler.guard-unavailable`) currently have no executed test coverage, and neither does anything else the
suite asserts.

The failing script is outside the active change's `## In scope`, which covered the claim script's validation
result path only.

Session provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Session: 1dbdee9b-549a-425c-8469-e6aa21b29f23
- Session branch: claude/1dbdee9b-549a-425c-8469-e6aa21b29f23
- Worktree: .claude\worktrees\BrokenEngine\1dbdee9b-549a-425c-8469-e6aa21b29f23
- Landing commit: `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/NextPlanWorkflowFixtureSessionSidecar.md`

`/next-plan-review` is not required here: the root cause is proven from current source at the citations above,
so this Plan is implementable directly from its `## Design`.

## Design

In `Test-NextPlanWorkflowScripts.ps1`, immediately after the fixture creates the session worktree
(`git worktree add -b $sessionBranch ...`, `:83`), write the sidecar the session module requires, using the
fixture's existing `Set-Utf8File` helper:

- Path: `Temp/session-sidecar.json` inside `$script:session`.
- Content: one JSON object with exactly the two properties the module validates at `:93-95` —
  `schemaVersion` = `broken-engine-session-sidecar/v1` and `targetBranch` = `main`, the fixture primary's
  branch (`:61`, `git init --initial-branch=main`).

`Temp/` is already in the fixture's `.gitignore` (`:65`), so the sidecar does not dirty the fixture worktree
and the suite's clean-tree assertions keep passing.

Nothing else changes: no assertion, no case ordering, and no script under test.

## Critical files

- `.agents/skills/next-plan/scripts/Test-NextPlanWorkflowScripts.ps1` — fixture setup around `:82-85`.
- `.agents/scripts/AgentWorktreeSession.psm1:75-101` — the sidecar contract being satisfied; not changed.

## In scope

- Writing the session sidecar during fixture setup in `Test-NextPlanWorkflowScripts.ps1`.
- Any purely consequential adjustment inside that same fixture setup needed for the suite to reach its first
  assertion.

## Out of scope

- `AgentWorktreeSession.psm1` and the sidecar requirement itself, including any fallback for a missing sidecar.
- `Defer-NextPlan.ps1`, `Complete-NextPlan.ps1`, `Invoke-NextPlanClaim.ps1`, `Get-NextPlanList.ps1`, and every
  other script under test.
- New test cases, including coverage for the `scheduler.busy` and `scheduler.guard-unavailable` branches: this
  Plan only makes the existing suite runnable.
- WorktreeCli, the wrapper session scripts, and `/next-plan`'s skill body.

## Risk tier and invariants

Expected Tier 1 (a fixture-local, behavior-preserving setup fix in one script; it exposes no public signature
or runtime invariant). Escalate only if making the suite run reveals that a script under test must change. The
fixture must keep isolating machine-local state — it already redirects `LOCALAPPDATA` (`:74-75`) — and must
never write into the real repository's `Temp` or claim store.

## Acceptance criteria

- The documented invocation runs past `Defer-NextPlan.ps1` and completes the whole suite, with the recorded
  `defer.context-conflict` failure no longer reproducing.
- The suite's own clean-worktree assertions still pass, showing the sidecar did not dirty the fixture session
  worktree.
- No file under the real `%LOCALAPPDATA%\BrokenEngineLocks` and no path outside the fixture root is created by
  the run.
