<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T17:10:00.905Z","dependsOn":[]} -->
# Fix: Invoke-FinalizeCandidateCommit.ps1 — a staged-then-modified owned Plan blocks the candidate

## Context

Observed symptom, in one session and one invocation:

- `pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1 ...`
  returned exit 2, `git.owned-path-mixed-state`, "Authorized path
  'Documents/Plans/Engine/FinalizeVerifiedCandidateWorktreeIdentity.md' has
  mixed index and worktree state. Stage it with 'git add -- <path>', or restore
  it, so its index and worktree agree, then rerun."
- How the mixed state arose: `/create-follow-up-plans` created that Plan through
  `.agents/scripts/New-PlanFile.ps1`, which deliberately stages the new Plan
  (`:163-164`, `git add -- :(literal)<plan>`). A later `/resolve-findings` round
  edited the same Plan in the worktree and did not restage it, so its index copy
  and its worktree copy disagreed.
- Rework it forced: the blocked invocation, a manual `git add -- <path>` exactly
  as the message prescribed, and a second invocation, which passed.

Behavior of the check and of the code around it, read from the current tree:

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1:32`
  — `Assert-OwnedNotMixed` blocks whenever an owned path's `git status`
  porcelain XY has both a non-blank, non-`?` index column and a non-blank,
  non-`?` worktree column.
- `:65` — the candidate itself is built in a temporary index seeded by
  `git read-tree <expected tip>` and then `git add -A -- <owned pathspecs>`,
  which takes the worktree bytes. The real index contributes nothing to the
  candidate tree for an owned path; after the session ref advances, `:65`
  reconciles the real index from the candidate tree with
  `Set-IndexPathFromTree`.
- `.agents/skills/finalize-changes/references/scripts.md:114-124` documents the
  guard and its remedy, so the block is the documented behavior, not a script
  bug in the sense of a broken contract.
- `Tools/WorktreeCli/PlanMetadata.cpp:174` enumerates Plans with
  `git ls-files -- Documents/Plans`, i.e. from the index, which is why
  `New-PlanFile.ps1` stages what it writes.

So the collision is between two correct-in-isolation behaviors: the Plan writer
stages, and a later worktree-only edit of that same Plan leaves the pair
disagreeing until someone restages.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 552a615b-9166-4c5b-8991-7e9c279d2dcf
- Worktree/branch UUID: def6cf77-44da-402b-b5e4-a597e25a7971
- Session branch: claude/def6cf77-44da-402b-b5e4-a597e25a7971
- Worktree: .claude\worktrees\BrokenEngine\def6cf77-44da-402b-b5e4-a597e25a7971
- Landing ref: claude/def6cf77-44da-402b-b5e4-a597e25a7971
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

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/def6cf77-44da-402b-b5e4-a597e25a7971` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Author's recommendation: relax `Assert-OwnedNotMixed` so that an owned path
whose index and worktree simply disagree — the staged-then-modified case, where
neither status column reports an unmerged state — is accepted rather than
blocked, and keep blocking every unmerged combination (`U` in either column, and
the `AA`/`DD` pairs `git status` uses for both-added and both-deleted
conflicts). Rationale: for an owned path the candidate is already defined by the
worktree bytes alone (`:65` reads the tree, then `git add -A` over the owned
pathspecs), so a staged-then-modified owned path has no ambiguity for the script
to protect the caller from, and the real index is rewritten from the candidate
tree immediately afterwards. A conflicted path is a different case, because its
worktree bytes may carry conflict markers, and it should keep blocking. The fix
also updates the guard's description in
`.agents/skills/finalize-changes/references/scripts.md` (`:114-124`) so the
documented behavior matches, keeping the remedy sentence for the states that
still block.

Alternatives considered and not recommended:

- Stop staging in `New-PlanFile.ps1`. Rejected: `Tools/WorktreeCli/PlanMetadata.cpp:174`
  lists Plans from the index, so an unstaged new Plan is invisible to the
  `plan validate` the script folds into its own result, and to any later
  scheduler check in the same worktree.
- Require `/resolve-findings`, or any worker editing an already-staged file, to
  restage it. Rejected: it puts a Git-index obligation into skill prose for every
  future editor of any staged path, to avoid one guard that the recommended fix
  removes at its source; it is also unenforceable, since nothing checks it until
  the finalizer blocks.
- Have the finalizer run `git add -- <path>` itself when it sees the mixed
  state. Rejected: it makes the candidate script mutate the real index outside
  its owned reconciliation step, which is the state it is otherwise careful to
  snapshot and roll back (`Get-IndexPathSnapshot`/`Restore-IndexPathSnapshot`
  at `:65`).

The fix session should confirm from the current bytes that the candidate path
still ignores the real index for owned paths before relaxing the guard; if that
has changed, the guard is load-bearing and the recommendation does not hold.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1`
  — `Assert-OwnedNotMixed` (`:32`) and the candidate construction it guards
  (`:65`).
- `.agents/skills/finalize-changes/references/scripts.md` — `:114-124`, the
  documented description of the guard and its remedy.
- `.agents/scripts/New-PlanFile.ps1` — `:163-164`, read as the unchanged reason
  a new Plan is staged.
- `Tools/WorktreeCli/PlanMetadata.cpp` — `:174`, read as the unchanged reason
  that staging is required.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to `Assert-OwnedNotMixed` in
  `Invoke-FinalizeCandidateCommit.ps1` and the matching guard description in
  `references/scripts.md`.

## Out of scope

- `New-PlanFile.ps1`, `Tools/WorktreeCli/`, and any change to how or whether a
  new Plan is staged.
- `/resolve-findings`, `/create-follow-up-plans`, and any new restaging
  obligation in skill prose.
- The candidate construction, the temporary index, the owned-path index
  snapshot and rollback, the result schema, and the other blocking codes.
- Primary advancement, the landing lock, and the confirmation gate.
- The landed change the session produced.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2 (scoped behavior of one tool script's input guard); escalate if
the fix reaches the candidate construction or primary advancement. Never embed
transcript paths or home paths.

## Acceptance criteria

The diff alone does not prove the behavior, so, in a scratch repository:

1. An owned path staged and then modified in the worktree no longer blocks, and
   the resulting candidate tree carries the worktree bytes for that path.
2. An owned path in an unmerged state still blocks with
   `git.owned-path-mixed-state` and its remedy message.
3. Unrelated (non-owned) index and worktree state is preserved exactly as
   before, per the script's existing `unrelatedPreserved` reporting.
4. `plan validate` exits 0, and /validate-skill passes wherever the root
   AGENTS.md Apply the triggered cleanup step triggers it.

## Notes

Change Workflow tier for the fix: **Tier 2** — scoped behavior of one tool
script, at an existing boundary, with no determinism/CRC, wire, serialization,
save/replay, or threading exposure and no trust-boundary change.
