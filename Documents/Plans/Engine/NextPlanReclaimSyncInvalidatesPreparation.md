<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T14:58:54.477Z","dependsOn":[]} -->
# Fix: /next-plan worker step 5 — a re-claim `sync` object leaves main with no rule for stale preparation evidence

## Context
`.agents/skills/next-plan/references/worker.md:55-56` (step 5) says to invoke the
claim script idempotently immediately before the final preparation handoff, and
is done "when it reports the held claim". In this session that run:

`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan '<claimed Plan path>'`

returned `"code":"reused"` together with
`"sync":{"fastForwarded":true,"from":"150fb3befb085cf8df4dadbb5edb50a9e1d72551","to":"fe5a6e3bd033f4184b5f265dd7b94d9729b9cdef"}`.
The preparation `implementer` had already verified the claimed Plan against
baseline `150fb3be`, so the fast-forward moved the tree underneath evidence that
was already gathered.

No documented rule covers that state. Step 5 (`worker.md:55-56`) names only the
held claim; the `## Rules` line at `worker.md:88` says only that the context
baseline is provisional until a claim reports a `sync` object; and
`.agents/skills/next-plan/references/claim-results.md:8-16` (`## Session
baseline and the sync object`) says only that the result's `sync.to` becomes the
session baseline. None of them says what main does about preparation evidence
gathered against `sync.from`.

Workaround actually performed: main improvised
`git diff --stat <from> <to> -- <the preparation handoff's cited paths>` plus
`git log --oneline <from>..<to>` to confirm the fast-forwarded range did not
touch the preparation's cited files, and only then continued. Had the range
intersected those paths, the preparation dispatch would have needed re-running,
with nothing in the documented workflow telling main so.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4065ff1e-f804-4dad-b064-788d80d777ea
- Worktree/branch UUID: ad63c478-92a8-4c3a-9c69-0d4557a38100
- Session branch: claude/ad63c478-92a8-4c3a-9c69-0d4557a38100
- Worktree: .claude\worktrees\BrokenEngine\ad63c478-92a8-4c3a-9c69-0d4557a38100
- Landing ref: claude/ad63c478-92a8-4c3a-9c69-0d4557a38100
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
The `## Context` above records the whole gap, so the fix session should
root-cause from the current tree and this Plan alone; run
`/next-plan-review claude/ad63c478-92a8-4c3a-9c69-0d4557a38100` in bounded
friction mode with client `claude` and the conversation session ID above only if
the tree turns out not to settle it.

As the author's recommendation rather than a binding decision: add one sentence
to `worker.md` step 5 stating that when the idempotent re-claim reports a `sync`
object, main diffs the fast-forwarded range against the preparation handoff's
cited paths and re-dispatches preparation when they intersect. The rationale is
that the check main already improvised is the missing rule, it needs no new
script or result field, and it lands at the one step where the stale-evidence
window opens. Touch the corresponding sentence of `claim-results.md`
(`:8-16`) only if leaving it unchanged would state the same fact twice.

An alternative the author rejects: moving the re-claim ahead of step 4's
preparation dispatch, so preparation always starts from the synced tree.
`claim-results.md:8-16` already positions step 3's claim as exactly that
provisional-baseline sync, so this would duplicate an existing sync rather than
close the window — step 5 exists precisely because primary can advance during
the preparation dispatch.

## Critical files
- `.agents/skills/next-plan/references/worker.md`
- `.agents/skills/next-plan/references/claim-results.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting documentation fix, confined to step 5 of
  `.agents/skills/next-plan/references/worker.md` and, only where the same fact
  would otherwise be stated twice, the `## Session baseline and the sync object`
  section of `.agents/skills/next-plan/references/claim-results.md`

## Out of scope
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` and every other
  claim, listing, or completion script
- The Git-backed scheduler and WorktreeCli
- Every other `/next-plan` worker step, and `next-plan/SKILL.md`
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation of an existing check inside one skill package,
with no public signature or invariant exposure). Escalate to Tier 2 if the fix
changes the claim script's behavior or the claim result shape instead of the
prose. Never embed transcript paths or home paths.

## Acceptance criteria
- `worker.md` step 5 states what main does when the re-claim reports a `sync`
  object, including the re-dispatch condition, and the same fact is stated in
  exactly one place across `worker.md` and `claim-results.md`
- /validate-skill passes for the `next-plan` package;
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`, `code: ok`
